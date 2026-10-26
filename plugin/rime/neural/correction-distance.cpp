// SPDX-License-Identifier: MIT
#include "typing-corrector.h"
#include <rime/algo/syllabifier.h>
#include <map>

// Exact libc++/arm64 symbol from the checksum-pinned Rime archive. --wrap
// redirects its callers here while keeping the original graph builder intact.
// CMake passes the same symbol to the linker; port both together on ABI updates.
#define CIME_SYLLABIFIER_SYMBOL "_ZN4rime11Syllabifier18BuildSyllableGraphERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEERNS_5PrismEPNS_13SyllableGraphE"
extern "C" int originalSyllabify(rime::Syllabifier *, const std::string &,
    rime::Prism &, rime::SyllableGraph *) asm("__real_" CIME_SYLLABIFIER_SYMBOL);
extern "C" int weightedSyllabify(rime::Syllabifier *, const std::string &,
    rime::Prism &, rime::SyllableGraph *) asm("__wrap_" CIME_SYLLABIFIER_SYMBOL);

extern "C" int weightedSyllabify(rime::Syllabifier *syllabifier, const std::string &input,
                                rime::Prism &prism, rime::SyllableGraph *graph) {
    const int farthest = originalSyllabify(syllabifier, input, prism, graph);
    auto *corrector = dynamic_cast<chinese_ime_lm::TypingCorrector *>(syllabifier->corrector());
    if (!corrector) return farthest;  // Enhanced correction is disabled for this schema.

    // The pinned builder discards Correction.distance and applies log(0.01) to
    // every corrected edge. Restore additive per-step costs before the poet or
    // dictionary sees the graph. Exact, fuzzy and abbreviation edges retain
    // their schema properties. Existing edge objects/indices stay valid.
    constexpr double stepCredibility = -4.605170185988091;  // log(0.01), Rime's one-step value.
    for (auto &[start, ends] : graph->edges) {
        rime::corrector::Corrections corrections;
        corrector->ToleranceSearch(prism, input.substr(start), &corrections, 5);
        std::map<std::pair<size_t, rime::SyllableId>, size_t> costs;
        for (const auto &[spelling, correction] : corrections) {
            if (correction.distance == 0) continue;
            size_t end = start + correction.length;
            int exact;
            if (prism.GetValue(input.substr(start, correction.length), &exact) && exact == spelling)
                continue;
            while (end < input.size() && syllabifier->delimiters().find(input[end]) != std::string::npos)
                ++end;
            for (auto accessor = prism.QuerySpelling(spelling); !accessor.exhausted(); accessor.Next()) {
                const auto properties = accessor.properties();
                if (properties.type != rime::kNormalSpelling || properties.is_correction) continue;
                const auto key = std::make_pair(end, accessor.syllable_id());
                const auto [found, inserted] = costs.emplace(key, correction.distance);
                if (!inserted) found->second = std::min(found->second, correction.distance);
            }
        }
        for (auto &[end, spellings] : ends) {
            for (auto &[syllable, properties] : spellings) {
                const auto cost = costs.find({end, syllable});
                if (properties.is_correction && cost != costs.end())
                    properties.credibility = stepCredibility * cost->second;
            }
        }
    }
    return farthest;
}
