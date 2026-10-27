// SPDX-License-Identifier: MIT
#pragma once
#include <rime/candidate.h>
#include <rime/gear/translator_commons.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace chinese_ime_lm {
namespace contextual_ranking_detail {
inline rime::an<rime::Phrase> dictionaryPhrase(
    const rime::an<rime::Candidate> &candidate) {
    const auto genuine = rime::Candidate::GetGenuineCandidate(candidate);
    const auto phrase = std::dynamic_pointer_cast<rime::Phrase>(genuine);
    if (!phrase || (phrase->type() != "phrase" && phrase->type() != "user_phrase") ||
        std::dynamic_pointer_cast<rime::Sentence>(phrase) || !phrase->is_exact_match())
        return {};
    // A text-changing wrapper no longer exposes the dictionary text being
    // compared. Keep its original ordering rather than score another reading.
    if (candidate->text() != phrase->text() || candidate->start() != phrase->start() ||
        candidate->end() != phrase->end()) return {};
    return phrase;
}
} // namespace contextual_ranking_detail

// With actual left context, compare only complete dictionary homophones on the
// leader's original syllable path. Empty means the dictionary protection stays
// in force. Bit 0 permits scoring; bit 1 (trusted dictionary) is intentionally
// absent only for this restricted group. Other candidates have no scoring bits.
// is_exact_match() rejects predictive completions; it does not mean that the
// spelling path was free of corrections or schema-defined fuzzy spellings.
inline std::vector<uint8_t> contextualPhraseFacts(
    const rime::CandidateList &candidates, size_t segmentStart, size_t inputEnd,
    bool hasContext) {
    if (!hasContext || candidates.empty()) return {};
    const auto leader = contextual_ranking_detail::dictionaryPhrase(candidates.front());
    if (!leader || leader->start() != segmentStart || leader->end() != inputEnd ||
        leader->code().empty()) return {};
    const auto leaderSpans = leader->spans();
    if (leaderSpans.Count() != leader->code().size() ||
        leaderSpans.start() != segmentStart || leaderSpans.end() != inputEnd) return {};

    std::vector<uint8_t> facts(candidates.size(), 0);
    bool hasAlternative = false;
    for (size_t index = 0; index < candidates.size(); ++index) {
        const auto phrase = contextual_ranking_detail::dictionaryPhrase(candidates[index]);
        if (!phrase || phrase->start() != segmentStart || phrase->end() != inputEnd ||
            phrase->language() != leader->language() || phrase->code() != leader->code())
            continue;
        const auto spans = phrase->spans();
        if (spans.Count() != leaderSpans.Count() || spans.start() != segmentStart ||
            spans.end() != inputEnd) continue;
        size_t position = segmentStart;
        bool sameSpans = true;
        for (size_t syllable = 0; syllable < leaderSpans.Count(); ++syllable) {
            const size_t end = leaderSpans.NextStop(position);
            if (spans.NextStop(position) != end) {
                sameSpans = false;
                break;
            }
            position = end;
        }
        if (!sameSpans) continue;
        facts[index] = 1;
        hasAlternative |= phrase->text() != leader->text();
    }
    return hasAlternative ? facts : std::vector<uint8_t>();
}
} // namespace chinese_ime_lm
