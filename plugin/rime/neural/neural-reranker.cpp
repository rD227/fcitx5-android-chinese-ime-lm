// SPDX-License-Identifier: MIT
#include "neural-reranker.h"
#include "candidate-policy.h"
#include "sentence-alternatives.h"
#include "typing-corrector.h"
#include "sentence-decoder.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <android/log.h>
#include <rime/candidate.h>
#include <rime/component.h>
#include <rime/config.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/dict/corrector.h>
#include <rime/filter.h>
#include <rime/registry.h>
#include <rime/schema.h>
#include <rime/translation.h>

extern "C" {
void *cime_load(const char *path);
void cime_free(void *handle);
int cime_best(void *handle, const char *context, const char *const *texts,
              const uint8_t *facts, size_t count);
}

namespace {
constexpr const char *kLogTag = "ChineseImeLm";
// The reference model compares at most nine full-key candidates.
constexpr size_t kCandidateCount = 9;
std::string userDataDirectory;
std::string sharedDataDirectory;

class NeuralCorrectorComponent final : public rime::Corrector::Component {
public:
    rime::Corrector *Create(const rime::Ticket &ticket) override {
        chinese_ime_lm::TypingCorrectionOptions options;
        if (ticket.schema) {
            auto *config = ticket.schema->config();
            config->GetBool("chinese_ime_lm/enable_transposition", &options.transpositions);
            options.neighbors = options.transpositions;  // Preserve existing opt-in behavior.
            config->GetBool("chinese_ime_lm/enable_keyboard_neighbors", &options.neighbors);
            config->GetBool("chinese_ime_lm/enable_native_correction", &options.native);
            config->GetBool("chinese_ime_lm/enable_multiple_typos", &options.multiple);
            config->GetBool("chinese_ime_lm/enable_distant_neighbors", &options.distant);
        }
        return options.transpositions || options.neighbors || !options.native
            ? static_cast<rime::Corrector *>(new chinese_ime_lm::TypingCorrector(options))
            : new rime::NearSearchCorrector;
    }
};

class BufferedTranslation final : public rime::Translation {
public:
    BufferedTranslation(rime::CandidateList candidates,
                        rime::an<rime::Translation> rest)
        : candidates_(std::move(candidates)), rest_(std::move(rest)) {
        set_exhausted(candidates_.empty() && rest_->exhausted());
    }

    bool Next() override {
        if (cursor_ < candidates_.size()) {
            ++cursor_;
        } else if (!rest_->exhausted()) {
            rest_->Next();
        }
        set_exhausted(cursor_ == candidates_.size() && rest_->exhausted());
        return !exhausted();
    }

    rime::an<rime::Candidate> Peek() override {
        return cursor_ < candidates_.size() ? candidates_[cursor_] : rest_->Peek();
    }

private:
    rime::CandidateList candidates_;
    rime::an<rime::Translation> rest_;
    size_t cursor_ = 0;
};

class ChineseImeLmFilter final : public rime::Filter {
public:
    explicit ChineseImeLmFilter(const rime::Ticket &ticket) : Filter(ticket) {
        // Unaliased Rime filters receive the generic "filter" namespace.
        // Our documented custom.yaml settings belong to this component;
        // explicit aliases continue to select their own configuration section.
        if (name_space_ == "filter") name_space_ = "chinese_ime_lm";
        engine_->schema()->config()->GetBool(name_space_ + "/aggressive", &aggressive_);
        engine_->schema()->config()->GetBool(name_space_ + "/enable_reranking", &reranking_);
        engine_->schema()->config()->GetBool(name_space_ + "/enable_sentence_alternatives", &alternatives_);
        auto clear = [this](rime::Context *) { decoderCache_.Clear(); };
        committed_ = engine_->context()->commit_notifier().connect(clear);
        aborted_ = engine_->context()->abort_notifier().connect(clear);
        deleted_ = engine_->context()->delete_notifier().connect(clear);
        if (!reranking_ && !aggressive_) return;
        std::string modelName;
        const bool customModel = engine_->schema()->config()->GetString(
            name_space_ + "/model", &modelName);
        const auto modelPath = customModel ? userDataDirectory + "/" + modelName
            : sharedDataDirectory + "/sentence-model.safetensors";
        model_.reset(cime_load(modelPath.c_str()));
        __android_log_print(model_ ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                            kLogTag, "model %s: %s", model_ ? "loaded" : "unavailable",
                            modelPath.c_str());
    }
    ~ChineseImeLmFilter() override {
        committed_.disconnect();
        aborted_.disconnect();
        deleted_.disconnect();
    }

    bool AppliesToSegment(rime::Segment *segment) override {
        return segment->HasTag("abc");
    }

    rime::an<rime::Translation> Apply(rime::an<rime::Translation> source,
                                     rime::CandidateList *) override {
        if (((reranking_ || aggressive_) && !model_) || source->exhausted() ||
            (!reranking_ && !alternatives_ && !aggressive_)) {
            return source;
        }
        const auto *ctx = engine_->context();
        const auto first = source->Peek();
        const auto genuine = rime::Candidate::GetGenuineCandidate(first);
        if (!genuine || genuine->type() != "sentence" ||
            first->end() != ctx->input().size()) {
            return source;
        }
        const auto input = ctx->input().substr(first->start(), first->end() - first->start());

        const auto started = std::chrono::steady_clock::now();
        rime::CandidateList candidates;
        std::vector<uint8_t> facts;
        while (!source->exhausted() && candidates.size() < kCandidateCount) {
            auto candidate = source->Peek();
            candidates.push_back(std::move(candidate));
            source->Next();
        }
        // Only already-selected segments of this composition are used as context.
        const auto context = ctx->composition().GetTextBefore(first->start());
        if (alternatives_)
            chinese_ime_lm::appendSentenceAlternatives(candidates, kCandidateCount - 1);
        std::vector<const char *> texts;
        for (const auto &candidate : candidates) {
            auto origin = rime::Candidate::GetGenuineCandidate(candidate);
            facts.push_back(chinese_ime_lm::candidateFacts(
                candidate->start(), candidate->end(), first->start(),
                ctx->input().size(), origin ? origin->type() : ""));
            texts.push_back(candidate->text().c_str());
        }
        // Only already-selected segments of this composition are used as context.
        // This does not read the clipboard, other apps, or previous input fields.
        const int promoted = reranking_ ? cime_best(model_.get(), context.c_str(), texts.data(),
                                                   facts.data(), texts.size()) : 0;
        if (promoted < 0 || static_cast<size_t>(promoted) >= candidates.size()) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag, "invalid inference result");
        } else if (promoted > 0) {
            // Move the actual Candidate object, preserving its input span and
            // dictionary identity so selection and learning follow the new order.
            std::rotate(candidates.begin(), candidates.begin() + promoted,
                        candidates.begin() + promoted + 1);
        }
        // The beam adds coverage AFTER the existing reranker has chosen the
        // leader. Direct 4M decoding regressed normal input in real probes;
        // generated alternatives must not displace that established decision.
        if (aggressive_) {
            chinese_ime_lm::appendDecodedSentences(candidates, model_.get(),
                input, context, decoderCache_);
        }
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        __android_log_print(ANDROID_LOG_DEBUG, kLogTag,
                            "evaluated=%zu beam_scored=%zu promoted=%d elapsed_us=%lld",
                            texts.size(), aggressive_ ? decoderCache_.evaluated : 0,
                            promoted, static_cast<long long>(micros));
        return rime::New<BufferedTranslation>(std::move(candidates), std::move(source));
    }

private:
    bool aggressive_ = true;
    bool reranking_ = true;
    bool alternatives_ = true;
    chinese_ime_lm::DecoderCache decoderCache_;
    rime::connection committed_, aborted_, deleted_;
    struct Deleter { void operator()(void *handle) const { cime_free(handle); } };
    std::unique_ptr<void, Deleter> model_;
};
} // namespace

void registerChineseImeLm(const char *directory, const char *sharedDirectory) {
    userDataDirectory = directory;
    sharedDataDirectory = sharedDirectory;
    rime::Registry::instance().Register(
        "chinese_ime_lm", new rime::Component<ChineseImeLmFilter>());
    rime::Registry::instance().Register("corrector", new NeuralCorrectorComponent);
    rime::Registry::instance().Register("script_translator",
        new rime::Component<chinese_ime_lm::DecodingTranslator>());
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Rime filter registered");
}
