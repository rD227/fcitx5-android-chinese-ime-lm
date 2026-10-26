// SPDX-License-Identifier: MIT
// Runs against the same Android prebuilt libraries as the APK. No user data.
#include "neural-reranker.h"
#include "sentence-alternatives.h"
#include "sentence-decoder.h"
#include "mixed-abbreviations.h"
#include <rime_api.h>
#include <rime/candidate.h>
#include <rime/component.h>
#include <rime/context.h>
#include <rime/menu.h>
#include <rime/registry.h>
#include <rime/segmentation.h>
#include <rime/service.h>
#include <rime/translation.h>
#include <rime/translator.h>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

long residentKb() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) return std::stol(line.substr(6));
    }
    throw std::runtime_error("Cannot read VmRSS");
}

class FixtureTranslator : public rime::Translator {
public:
    explicit FixtureTranslator(const rime::Ticket &ticket) : Translator(ticket) {}
    rime::an<rime::Translation> Query(const std::string &input,
                                     const rime::Segment &segment) override {
        auto result = rime::New<rime::FifoTranslation>();
        if (input == "expand") {
            rime::DictEntry prefix;
            prefix.text = "我在";
            prefix.code.assign({1, 2});
            rime::DictEntry tail;
            tail.text = "用用看";
            tail.code.assign({3, 3, 4});
            auto sentence = rime::New<rime::Sentence>(nullptr);
            sentence->Extend(prefix, 3, 0);
            sentence->Extend(tail, 6, 0);
            sentence->Offset(segment.start);
            result->Append(sentence);
            prefix.text = "我再";
            result->Append(rime::New<rime::Phrase>(nullptr, "phrase", segment.start,
                segment.start + 3, rime::New<rime::DictEntry>(prefix)));
            return result;
        }
        result->Append(rime::New<rime::SimpleCandidate>(
            input == "dict" ? "phrase" : "sentence", segment.start, segment.end,
            "今天天气真号"));
        result->Append(rime::New<rime::SimpleCandidate>(
            "sentence", segment.start, segment.end, "今天天气真好"));
        // Partial-key candidates must remain reachable after the nine-item buffer.
        for (int index = 0; index < 12; ++index) {
            result->Append(rime::New<rime::SimpleCandidate>(
                "phrase", segment.start, segment.start + 1,
                "候选" + std::to_string(index)));
        }
        return result;
    }
};

void checkAlternatives() {
    chinese_ime_lm::MixedAbbreviationSyllabifier mixed(3, 4, 7);
    const auto mixedSpans = mixed.Syllabify(nullptr);
    require(mixedSpans.Count() == 2 && mixedSpans.start() == 3 && mixedSpans.end() == 7 &&
            mixedSpans.NextStop(3) == 4 && mixedSpans.NextStop(4) == 7,
            "Mixed abbreviation lost the actual input boundaries at a nonzero offset");
    rime::DictEntry prefix;
    prefix.text = "我在";
    prefix.code.assign({1, 2});
    rime::DictEntry tail;
    tail.text = "用用看";
    tail.code.assign({3, 3, 4});
    auto base = rime::New<rime::Sentence>(nullptr);
    base->Extend(prefix, 5, 0);
    base->Extend(tail, 16, 0);
    base->Offset(2);
    prefix.text = "我再";
    auto valid = rime::New<rime::Phrase>(nullptr, "phrase", 2, 7,
        rime::New<rime::DictEntry>(prefix));
    prefix.text = "我最爱";
    prefix.code.assign({1, 6, 7});
    auto wrongPath = rime::New<rime::Phrase>(nullptr, "phrase", 2, 7,
        rime::New<rime::DictEntry>(prefix));
    prefix.code.assign({1, 2});
    auto partialWord = rime::New<rime::Phrase>(nullptr, "phrase", 2, 6,
        rime::New<rime::DictEntry>(prefix));
    auto completion = rime::New<rime::Phrase>(nullptr, "completion", 2, 7,
        rime::New<rime::DictEntry>(prefix));
    rime::CandidateList candidates{base, valid, wrongPath, partialWord, completion};
    chinese_ime_lm::appendSentenceAlternatives(candidates, 8);
    require(candidates.size() == 6, "Wrong path, partial word or completion expanded");
    const auto expanded = std::dynamic_pointer_cast<rime::Sentence>(candidates.back());
    require(expanded && expanded->text() == "我再用用看", "Homophone expansion missing");
    require(expanded->start() == 2 && expanded->end() == 18, "Expanded input span changed");
    require(expanded->code() == base->code(), "Expanded syllable path changed");
    require(expanded->components()[0].text == "我再" &&
            expanded->components()[1].text == "用用看", "Expanded learning entries changed");
    require(base->text() == "我在用用看", "Expansion mutated the original sentence");
    require(candidates[1] == valid, "Expansion replaced the original prefix candidate");
}

void writeSchema(const std::string &directory, const char *id, bool neural) {
    std::ofstream file(directory + "/" + id + ".schema.yaml");
    file << "schema:\n  schema_id: " << id << "\n  name: smoke\n  version: '1'\n"
            "menu:\n  page_size: 20\n"
            "engine:\n  processors: [speller, selector, express_editor]\n"
            "  segmentors: [abc_segmentor, fallback_segmentor]\n"
            "  translators: [neural_fixture]\n  filters: [";
    if (neural) file << "chinese_ime_lm, ";
    file << "uniquifier]\nspeller:\n  alphabet: abcdefghijklmnopqrstuvwxyz\n";
    require(static_cast<bool>(file), "Cannot write fixture schema");
}

void check(RimeApi *api, RimeSessionId session, const char *input,
           const char *expected) {
    api->clear_composition(session);
    for (const char *key = input; *key; ++key) api->process_key(session, *key, 0);
    RIME_STRUCT(RimeContext, context);
    require(api->get_context(session, &context), "Cannot read Rime context");
    require(context.menu.num_candidates == 14, "Candidate tail lost or duplicated");
    require(context.menu.candidates[0].text == std::string(expected), "Wrong leader");
    api->free_context(&context);
    require(api->select_candidate(session, 0), "Cannot select leader");
    RIME_STRUCT(RimeCommit, commit);
    require(api->get_commit(session, &commit), "Selected candidate did not commit");
    require(commit.text == std::string(expected), "Selected candidate changed identity");
    api->free_commit(&commit);
}

int checkSpellings(const std::string &cases) {
    // RimeIce has one script translator. Inspect the actual loaded prism and
    // corrector, rather than crediting test YAML aliases to the native search.
    require(chinese_ime_lm::decodingTranslators.size() == 1,
            "Spelling fixture must have exactly one script translator");
    auto *translator = chinese_ime_lm::decodingTranslators.begin()->second;
    auto prism = translator->dict()->prism();
    require(prism && translator->corrector(), "Spelling fixture has no corrector");
    auto normalSyllables = [&](int spelling) {
        std::set<rime::SyllableId> result;
        for (auto accessor = prism->QuerySpelling(spelling); !accessor.exhausted(); accessor.Next()) {
            const auto properties = accessor.properties();
            if (properties.type == rime::kNormalSpelling && !properties.is_correction)
                result.insert(accessor.syllable_id());
        }
        return result;
    };
    std::ifstream file(cases);
    require(static_cast<bool>(file), "Cannot open spelling cases");
    std::string line;
    size_t count = 0, covered = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream columns(line);
        std::string kind, input, expected;
        require(static_cast<bool>(columns >> kind >> input >> expected), "Malformed spelling case");
        if (kind == "kind") continue;
        int spelling;
        require(prism->GetValue(expected, &spelling), "Expected syllable missing from fixture prism");
        const auto wanted = normalSyllables(spelling);
        require(!wanted.empty(), "Expected spelling is not a normal full syllable");
        bool found = false;
        size_t distance = 0;
        auto matches = [&](int candidate) {
            for (auto syllable : normalSyllables(candidate))
                if (wanted.count(syllable)) return true;
            return false;
        };
        if (prism->GetValue(input, &spelling)) found = matches(spelling);
        rime::corrector::Corrections corrections;
        translator->corrector()->ToleranceSearch(*prism, input, &corrections, 5);
        for (const auto &[key, correction] : corrections) {
            if (correction.length == input.size() && matches(key) &&
                (!found || correction.distance < distance)) {
                found = true;
                distance = correction.distance;
            }
        }
        if (found && distance > 0) {
            rime::SyllableGraph graph;
            rime::Syllabifier syllabifier("'", false, false);
            syllabifier.EnableCorrection(translator->corrector());
            syllabifier.BuildSyllableGraph(input, *prism, &graph);
            bool weighted = false;
            const auto starts = graph.edges.find(0);
            if (starts != graph.edges.end()) {
                const auto end = starts->second.find(input.size());
                if (end != starts->second.end()) {
                    for (auto syllable : wanted) {
                        const auto edge = end->second.find(syllable);
                        if (edge != end->second.end() && edge->second.is_correction &&
                            std::abs(edge->second.credibility + 4.605170185988091 * distance) < 1e-5)
                            weighted = true;
                    }
                }
            }
            require(weighted, "Correction distance lost before dictionary/poet scoring");
        }
        ++count;
        covered += found == (kind != "reject");
        std::cout << "SPELLING input=" << input << " expected=" << expected
                  << " covered=" << found << " distance=" << distance
                  << " wanted=" << (kind != "reject") << std::endl;
    }
    std::cout << "SPELLINGS passed=" << covered << " total=" << count << std::endl;
    require(count > 0, "No spelling cases read");
    return covered == count ? 0 : 1;
}

int probe(int argc, char **argv) {
    require(argc >= 6, "--probe <copied-user-directory> <shared-directory> <schema> <input>...");
    const bool selectExpected = std::string(argv[1]) == "--check-composition";
    if (selectExpected) require(argc == 7, "--check-composition <copy> <shared> <schema> <input> <expected>");
    auto *api = rime_get_api();
    RIME_STRUCT(RimeTraits, traits);
    traits.user_data_dir = argv[2];
    traits.shared_data_dir = argv[3];
    traits.log_dir = "";
    api->setup(&traits);
    api->initialize(&traits);
    api->deployer_initialize(&traits);
    registerChineseImeLm(argv[2], argv[3]);
    const std::string schema = argv[4];
    require(api->deploy_schema((std::string(argv[2]) + "/" + schema + ".schema.yaml").c_str()),
            "Probe schema deployment failed");
    const auto session = api->create_session();
    require(session != 0 && api->select_schema(session, schema.c_str()), "Probe session failed");
    api->set_option(session, "ascii_mode", false);
    if (std::string(argv[1]) == "--check-spellings") {
        require(argc == 6, "--check-spellings <copy> <shared> <schema> <tsv>");
        const int result = checkSpellings(argv[5]);
        api->destroy_session(session);
        api->finalize();
        return result;
    }
    for (int index = 5; index < argc - (selectExpected ? 1 : 0); ++index) {
        api->clear_composition(session);
        const auto begin = std::chrono::steady_clock::now();
        long long maxKeyMicros = 0;
        for (const char *key = argv[index]; *key; ++key) {
            const auto keyBegin = std::chrono::steady_clock::now();
            api->process_key(session, *key, 0);
            // Fcitx reads the context after each key. Force the same lazy menu
            // preparation so the timing and decoder cache reflect that path.
            RIME_STRUCT(RimeContext, keyContext);
            if (api->get_context(session, &keyContext)) api->free_context(&keyContext);
            maxKeyMicros = std::max(maxKeyMicros, static_cast<long long>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - keyBegin).count()));
        }
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - begin).count();
        auto *ctx = rime::Service::instance().GetSession(session)->context();
        std::cout << "PROBE input=" << argv[index] << " total_us=" << micros
                  << " max_key_us=" << maxKeyMicros << std::endl;
        if (!ctx->composition().empty() && ctx->composition().back().menu) {
            auto menu = ctx->composition().back().menu;
            const size_t limit = std::string(argv[1]) == "--probe" ? 16 : 64;
            const size_t count = menu->Prepare(limit);
            size_t expectedIndex = count;
            for (size_t candidateIndex = 0; candidateIndex < count && candidateIndex < limit; ++candidateIndex) {
                const auto candidate = menu->GetCandidateAt(candidateIndex);
                const auto origin = rime::Candidate::GetGenuineCandidate(candidate);
                if (selectExpected && candidate->text() == argv[6]) {
                    expectedIndex = candidateIndex;
                    if (const auto phrase = std::dynamic_pointer_cast<rime::Phrase>(origin)) {
                        const auto spans = phrase->spans();
                        require(spans.Count() == phrase->code().size() &&
                                spans.start() == phrase->start() && spans.end() == phrase->end(),
                                "Selected phrase syllables and original input span disagree");
                    }
                }
                if (auto sentence = std::dynamic_pointer_cast<rime::Sentence>(origin)) {
                    require(sentence->spans().Count() == sentence->code().size(),
                            "Sentence lost its real syllable spans");
                    rime::Code code;
                    size_t length = 0;
                    for (size_t i = 0; i < sentence->components().size(); ++i) {
                        const auto &entry = sentence->components()[i];
                        code.insert(code.end(), entry.code.begin(), entry.code.end());
                        length += sentence->word_lengths()[i];
                    }
                    require(code == sentence->code() &&
                            length == sentence->end() - sentence->start(),
                            "Sentence learning entries and input span disagree");
                }
                std::cout << "CAND " << candidateIndex << " " << origin->type() << " "
                          << candidate->start() << ":" << candidate->end() << " "
                          << candidate->text() << std::endl;
                if (limit > 16) {
                    if (auto sentence = std::dynamic_pointer_cast<rime::Sentence>(origin)) {
                        std::cout << "DETAIL " << candidateIndex << " weight=" << sentence->weight();
                        for (const auto &entry : sentence->components())
                            std::cout << " [" << entry.text << ":" << entry.weight << "]";
                        std::cout << std::endl;
                    }
                }
            }
            if (selectExpected) {
                require(expectedIndex < count, "Expected complete sentence not generated");
                require(api->select_candidate(session, expectedIndex), "Cannot select decoded sentence");
                RIME_STRUCT(RimeCommit, commit);
                require(api->get_commit(session, &commit), "Decoded sentence did not commit");
                require(std::string(commit.text) == argv[6], "Decoded selection changed text identity");
                api->free_commit(&commit);
                std::cout << "PASS: decoded sentence selected and committed" << std::endl;
            }
        }
    }
    api->destroy_session(session);
    api->finalize();
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && (std::string(argv[1]) == "--probe" ||
                    std::string(argv[1]) == "--probe-wide" ||
                    std::string(argv[1]) == "--check-spellings" ||
                    std::string(argv[1]) == "--check-composition")) return probe(argc, argv);
    require(argc == 3, "Usage: rime-neural-smoke <scratch-directory> <model-directory>");
    checkAlternatives();
    const std::string scratch = argv[1];
    std::filesystem::create_directories(scratch + "/build");
    writeSchema(scratch, "baseline", false);
    writeSchema(scratch, "neural", true);
    // These artificial sentences test candidate identity in isolation. Earlier
    // fixture commits must not alter the model's next comparison.
    std::ofstream(scratch + "/neural.schema.yaml", std::ios::app) <<
        "chinese_ime_lm:\n  use_commit_history: false\n";
    writeSchema(scratch, "disabled", true);
    std::ofstream(scratch + "/disabled.schema.yaml", std::ios::app) <<
        "chinese_ime_lm:\n  enable_reranking: false\n"
        "  enable_sentence_alternatives: false\n  aggressive: false\n";
    std::ofstream(scratch + "/default.yaml") <<
        "config_version: '1'\nschema_list:\n  - schema: baseline\n";

    auto *api = rime_get_api();
    std::cout << "Rime version=" << api->get_version() << std::endl;
    require(std::string(api->get_version()) == "1.16.1", "Rime header/library mismatch");
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = scratch.c_str();
    traits.user_data_dir = scratch.c_str();
    traits.log_dir = "";
    traits.min_log_level = 3;
    api->setup(&traits);
    api->initialize(&traits);
    api->deployer_initialize(&traits);
    registerChineseImeLm(scratch.c_str(), argv[2]);
    rime::Registry::instance().Register(
        "neural_fixture", new rime::Component<FixtureTranslator>());
    require(api->deploy_schema((scratch + "/baseline.schema.yaml").c_str()), "Deploy baseline failed");
    require(api->deploy_schema((scratch + "/neural.schema.yaml").c_str()), "Deploy neural failed");
    require(api->deploy_schema((scratch + "/disabled.schema.yaml").c_str()), "Deploy disabled failed");
    const auto session = api->create_session();
    require(session != 0, "Cannot create Rime session");
    require(api->select_schema(session, "baseline"), "Cannot select baseline");
    check(api, session, "abc", "今天天气真号");
    const long before = residentKb();
    require(api->select_schema(session, "neural"), "Cannot create neural filter");
    const long loaded = residentKb();
    check(api, session, "abc", "今天天气真好");
    check(api, session, "dict", "今天天气真号");
    api->clear_composition(session);
    for (const char key : std::string("expand")) api->process_key(session, key, 0);
    RIME_STRUCT(RimeContext, expandedContext);
    require(api->get_context(session, &expandedContext), "Cannot read expanded composition");
    require(expandedContext.menu.num_candidates == 3, "Expanded candidate count wrong");
    require(std::string(expandedContext.menu.candidates[0].text) == "我再用用看",
            "Model did not select the new full-sentence alternative");
    api->free_context(&expandedContext);
    require(api->select_candidate(session, 0), "Cannot select expanded sentence");
    RIME_STRUCT(RimeCommit, expandedCommit);
    require(api->get_commit(session, &expandedCommit), "Expanded sentence did not commit");
    require(std::string(expandedCommit.text) == "我再用用看", "Expanded commit changed identity");
    api->free_commit(&expandedCommit);
    const long after = residentKb();
    std::cout << "RSS before_kb=" << before << " loaded_kb=" << loaded
              << " after_inference_kb=" << after << " delta_kb=" << after - before
              << std::endl;
    require(api->select_schema(session, "disabled"), "Cannot select disabled fixture");
    check(api, session, "abc", "今天天气真号");
    api->clear_composition(session);
    for (const char key : std::string("expand")) api->process_key(session, key, 0);
    RIME_STRUCT(RimeContext, disabledContext);
    require(api->get_context(session, &disabledContext), "Cannot read disabled fixture");
    require(disabledContext.menu.num_candidates == 2 &&
            std::string(disabledContext.menu.candidates[0].text) == "我在用用看",
            "Custom false flags did not disable reranking/expansion");
    api->free_context(&disabledContext);
    api->destroy_session(session);
    api->finalize();
    std::cout << "PASS: filter ABI, generated sentence promotion, dictionary protection, "
                 "candidate tail and selected candidate commit" << std::endl;
}
