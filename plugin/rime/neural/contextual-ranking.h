// SPDX-License-Identifier: MIT
#pragma once
#include <rime/candidate.h>
#include <rime/algo/syllabifier.h>
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

// A different reading is comparable only where both paths expand the same
// original abbreviation. Normal/fuzzy/corrected spelling changes stay excluded.
inline bool contextualAbbreviations(const rime::Code &leader, const rime::Code &other,
                                    const rime::Spans &spans, size_t segmentStart,
                                    const rime::SyllableGraph &graph) {
    if (leader.size() != other.size()) return false;
    size_t position = segmentStart;
    for (size_t index = 0; index < leader.size(); ++index) {
        const size_t end = spans.NextStop(position);
        if (leader[index] != other[index]) {
            const auto startEdge = graph.edges.find(position - segmentStart);
            if (startEdge == graph.edges.end()) return false;
            const auto endEdge = startEdge->second.find(end - segmentStart);
            if (endEdge == startEdge->second.end()) return false;
            const auto first = endEdge->second.find(leader[index]);
            const auto second = endEdge->second.find(other[index]);
            if (first == endEdge->second.end() || second == endEdge->second.end() ||
                first->second.type != rime::kAbbreviation ||
                second->second.type != rime::kAbbreviation ||
                first->second.is_correction || second->second.is_correction) return false;
        }
        position = end;
    }
    return true;
}

// With left context, compare complete dictionary homophones or readings that
// expand the same native abbreviation spans. Empty keeps dictionary protection
// in force. Bit 0 permits scoring; bit 1 (trusted dictionary) is intentionally
// absent only for this restricted group. Other candidates have no scoring bits.
// is_exact_match() rejects predictive completions; it does not mean that the
// spelling path was free of corrections or schema-defined fuzzy spellings.
inline std::vector<uint8_t> contextualPhraseFacts(
    const rime::CandidateList &candidates, size_t segmentStart, size_t inputEnd,
    bool hasContext, const rime::SyllableGraph *graph = nullptr) {
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
            phrase->language() != leader->language() ||
            phrase->code().size() != leader->code().size())
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
        if (phrase->code() != leader->code() &&
            (!graph || !contextualAbbreviations(leader->code(), phrase->code(),
                                               leaderSpans, segmentStart, *graph))) continue;
        facts[index] = 1;
        hasAlternative |= phrase->text() != leader->text();
    }
    return hasAlternative ? facts : std::vector<uint8_t>();
}
} // namespace chinese_ime_lm
