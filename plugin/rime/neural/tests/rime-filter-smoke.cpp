// SPDX-License-Identifier: GPL-3.0-or-later
// Runs against the same Android prebuilt libraries as the APK. No user data.
#include "neural-reranker.h"
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
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

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

int probe(int argc, char **argv) {
    require(argc >= 6, "--probe <copied-user-directory> <shared-directory> <schema> <input>...");
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
    for (int index = 5; index < argc; ++index) {
        api->clear_composition(session);
        const auto begin = std::chrono::steady_clock::now();
        for (const char *key = argv[index]; *key; ++key) api->process_key(session, *key, 0);
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - begin).count();
        auto *ctx = rime::Service::instance().GetSession(session)->context();
        std::cout << "PROBE input=" << argv[index] << " total_us=" << micros << std::endl;
        if (!ctx->composition().empty() && ctx->composition().back().menu) {
            auto menu = ctx->composition().back().menu;
            const size_t count = menu->Prepare(16);
            for (size_t candidateIndex = 0; candidateIndex < count && candidateIndex < 16; ++candidateIndex) {
                const auto candidate = menu->GetCandidateAt(candidateIndex);
                const auto origin = rime::Candidate::GetGenuineCandidate(candidate);
                std::cout << "CAND " << candidateIndex << " " << origin->type() << " "
                          << candidate->start() << ":" << candidate->end() << " "
                          << candidate->text() << std::endl;
            }
        }
    }
    api->destroy_session(session);
    api->finalize();
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--probe") return probe(argc, argv);
    require(argc == 3, "Usage: rime-neural-smoke <scratch-directory> <model-directory>");
    const std::string scratch = argv[1];
    std::filesystem::create_directories(scratch + "/build");
    writeSchema(scratch, "baseline", false);
    writeSchema(scratch, "neural", true);
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
    const auto session = api->create_session();
    require(session != 0, "Cannot create Rime session");
    require(api->select_schema(session, "baseline"), "Cannot select baseline");
    check(api, session, "abc", "今天天气真号");
    const long before = residentKb();
    require(api->select_schema(session, "neural"), "Cannot create neural filter");
    const long loaded = residentKb();
    check(api, session, "abc", "今天天气真好");
    check(api, session, "dict", "今天天气真号");
    const long after = residentKb();
    std::cout << "RSS before_kb=" << before << " loaded_kb=" << loaded
              << " after_inference_kb=" << after << " delta_kb=" << after - before
              << std::endl;
    api->destroy_session(session);
    api->finalize();
    std::cout << "PASS: filter ABI, generated sentence promotion, dictionary protection, "
                 "candidate tail and selected candidate commit" << std::endl;
}
