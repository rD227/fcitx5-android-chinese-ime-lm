// SPDX-License-Identifier: MIT
#pragma once
#include <rime/dict/corrector.h>
#include <rime/dict/dictionary.h>
#include <rime/dict/user_dictionary.h>
#include <rime/gear/poet.h>
#include <rime/gear/script_translator.h>
#include <algorithm>
#include <android/log.h>
#include <map>
#include <set>

extern "C" int cime_scores(void *, const char *, const char *const *, size_t, float *);

namespace chinese_ime_lm {
class DecodingTranslator;
inline std::map<const rime::Language *, DecodingTranslator *> decodingTranslators;

// Use the existing translator's dictionaries and corrector. Creating another
// Memory/translator in a filter would subscribe to commits twice and learn twice.
class DecodingTranslator final : public rime::ScriptTranslator {
public:
    explicit DecodingTranslator(const rime::Ticket &ticket) : ScriptTranslator(ticket) {
        decodingTranslators[language()] = this;
    }
    ~DecodingTranslator() override { decodingTranslators.erase(language()); }
    rime::Corrector *corrector() const { return corrector_.get(); }
};

class DecodedSentence final : public rime::Sentence {
public:
    explicit DecodedSentence(const rime::Phrase &base) : Sentence(base.language()) {
        static_cast<rime::Phrase &>(*this) = base;
        entry_ = rime::New<rime::DictEntry>();
        set_type("sentence");
        set_start(0);
        set_end(0);
    }
};

struct DecoderPath {
    rime::an<rime::Sentence> sentence;
};
struct DecoderCache {
    struct Beam {
        std::string signature;
        std::vector<DecoderPath> paths;
    };
    std::string input;
    std::string context;
    bool useUserDict = false;
    std::map<size_t, Beam> beams;
    size_t evaluated = 0;
    void Clear() { input.clear(); context.clear(); beams.clear(); }
};

// Generate alternate word boundaries on the chosen full-pinyin path. Actual
// dictionary entries retain the original language, syllabifier and learning.
inline void appendDecodedSentences(rime::CandidateList &candidates, void *model,
                                   const std::string &input,
                                   const std::string &context, DecoderCache &cache) {
    cache.evaluated = 0;
    const auto base = std::dynamic_pointer_cast<rime::Phrase>(
        rime::Candidate::GetGenuineCandidate(candidates.front()));
    if (!base) return;
    const auto found = decodingTranslators.find(base->language());
    if (found == decodingTranslators.end()) return;  // Other translator kinds.
    auto *translator = found->second;
    rime::Syllabifier syllabifier(translator->delimiters(),
        translator->enable_completion(), translator->strict_spelling());
    syllabifier.EnableCorrection(translator->corrector());
    rime::SyllableGraph graph;
    if (static_cast<size_t>(syllabifier.BuildSyllableGraph(
            input, *translator->dict()->prism(), &graph)) != input.size()) return;

    // Expand word boundaries on the current full-pinyin syllable segmentation.
    // Allowing abbreviation paths here invents extra syllables (e.g. w+o ->
    // "我哦"), overwhelms the beam, and makes a prefix LM prefer unrelated text.
    const auto spans = base->spans();
    if (spans.Count() != base->code().size()) return;
    rime::SyllableGraph bounded;
    std::map<size_t, std::string> signatures;
    std::string signature;
    bounded.input_length = bounded.interpreted_length = input.size();
    size_t start = 0;
    for (const auto id : base->code()) {
        const size_t end = spans.NextStop(start + base->start()) - base->start();
        const auto &spellings = graph.edges.at(start).at(end);
        const auto original = spellings.find(id);
        if (original == spellings.end() || original->second.type > rime::kFuzzySpelling)
            return;  // Still typing an abbreviated/incomplete syllable.
        // Recombine WORD boundaries along this proven syllable path. Other
        // corrected readings remain Rime's original candidates, with its priors.
        bounded.edges[start][end][id] = original->second;
        signature += ":" + std::to_string(id) + ":" +
            std::to_string(original->second.credibility);
        signature += "/" + input.substr(start, end - start);
        signatures[end] = signature;
        bounded.vertices[start] = bounded.vertices[end] = rime::kNormalSpelling;
        start = end;
    }
    for (auto &[start, ends] : bounded.edges)
        for (auto &[end, spellings] : ends)
            for (auto &[syllable, properties] : spellings)
                bounded.indices[start][syllable].push_back(&properties);

    // Keep a small partial beam for per-keystroke latency; retain a wider
    // completed pool so alternatives are still available for manual selection.
    constexpr size_t beamWidth = 4;
    constexpr size_t homophonesPerSpan = 8;
    using Sentences = std::vector<DecoderPath>;
    std::map<size_t, Sentences> beams;
    beams[0].push_back({rime::New<DecodedSentence>(*base)});
    const bool useUserDict = translator->user_dict() &&
        translator->user_dict()->loaded() && !translator->IsUserDictDisabledFor(input);
    if (context != cache.context || useUserDict != cache.useUserDict ||
        input.compare(0, cache.input.size(), cache.input) != 0)
        cache.Clear();
    auto cached = std::move(cache.beams);
    cache.beams.clear();
    cache.input = input;
    cache.context = context;
    cache.useUserDict = useUserDict;

    for (auto position = beams.begin(); position != beams.end(); ++position) {
        const size_t start = position->first;
        auto &sentences = position->second;
        const size_t width = start == input.size() ? 32 : beamWidth;
        const auto previous = cached.find(start);
        const bool reuse = start != 0 && previous != cached.end() &&
            previous->second.signature == signatures.at(start);
        if (reuse) {
            sentences.clear();
            for (const auto &path : previous->second.paths) {
                // Rebind cached dictionary entries to THIS query's syllabifier.
                // Reusing its old Sentence would retain the old input graph.
                auto sentence = rime::New<DecodedSentence>(*base);
                size_t end = 0;
                for (size_t i = 0; i < path.sentence->components().size(); ++i) {
                    end += path.sentence->word_lengths()[i];
                    sentence->Extend(path.sentence->components()[i], end, path.sentence->weight());
                }
                sentences.push_back({std::move(sentence)});
                if (sentences.size() == width) break;
            }
        }
        if (start != 0 && !reuse) {
            // Deduplicate equivalent text using its strongest dictionary path.
            std::stable_sort(sentences.begin(), sentences.end(), [](const auto &a, const auto &b) {
                return a.sentence->weight() > b.sentence->weight();
            });
            std::set<std::string> seen;
            sentences.erase(std::remove_if(sentences.begin(), sentences.end(),
                [&](const auto &path) { return !seen.insert(path.sentence->text()).second; }),
                sentences.end());
            std::vector<const char *> texts;
            for (const auto &path : sentences) texts.push_back(path.sentence->text().c_str());
            std::vector<float> scores(texts.size());
            if (cime_scores(model, context.c_str(), texts.data(), texts.size(), scores.data()) != 0) {
                __android_log_print(ANDROID_LOG_ERROR, "ChineseImeLm", "decoder scoring failed");
                return;
            }
            cache.evaluated += texts.size();
            std::vector<size_t> order(sentences.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(),
                [&](size_t a, size_t b) { return scores[a] > scores[b]; });
            Sentences retained;
            for (const size_t index : order) {
                retained.push_back(sentences[index]);
                if (retained.size() == width) break;
            }
            sentences = std::move(retained);
        }
        if (start != 0) cache.beams[start] = {signatures.at(start), sentences};
        if (start == input.size()) break;
        std::map<size_t, std::vector<rime::an<rime::DictEntry>>> words;
        auto collect = [&](auto result) {
            if (!result) return;
            for (auto &[end, entries] : *result) {
                size_t count = 0;
                while (!entries.exhausted() && count++ < homophonesPerSpan) {
                    auto entry = entries.Peek();
                    if (entry->IsExactMatch()) words[end].push_back(entry);
                    entries.Next();
                }
            }
        };
        collect(translator->dict()->Lookup(bounded, start, &translator->blacklist(), false));
        if (useUserDict) collect(translator->user_dict()->Lookup(bounded, start));
        for (auto &[end, entries] : words) {
            std::stable_sort(entries.begin(), entries.end(),
                [](const auto &a, const auto &b) { return a->weight > b->weight; });
            std::set<std::string> unique;
            size_t count = 0;
            for (const auto &entry : entries) {
                if (!unique.insert(entry->text).second) continue;
                for (const auto &prefix : sentences) {
                    auto sentence = rime::New<rime::Sentence>(*prefix.sentence);
                    sentence->Extend(*entry, end, prefix.sentence->weight() + entry->weight);
                    beams[end].push_back({std::move(sentence)});
                }
                if (++count == homophonesPerSpan) break;
            }
        }
    }
    const auto completed = beams.find(input.size());
    if (completed == beams.end()) return;
    std::set<std::string> seen;
    for (const auto &candidate : candidates) seen.insert(candidate->text());
    size_t inserted = 1;
    for (const auto &path : completed->second) {
        const auto &sentence = path.sentence;
        if (!seen.insert(sentence->text()).second) continue;
        auto candidate = rime::New<rime::Sentence>(*sentence);
        candidate->Offset(base->start());
        candidate->set_preedit(base->preedit());
        // Keep the actual syllabifier: selection, cursor spans and learning use it.
        candidates.insert(candidates.begin() + inserted++, candidate);
    }
}
}  // namespace chinese_ime_lm
