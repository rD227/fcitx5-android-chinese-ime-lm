// SPDX-License-Identifier: MIT
#pragma once
#include "sentence-decoder.h"
#include <rime/gear/translator_commons.h>
#include <algorithm>
#include <cmath>
#include <set>

namespace chinese_ime_lm {
class MixedAbbreviationSyllabifier final : public rime::PhraseSyllabifier {
public:
    MixedAbbreviationSyllabifier(size_t start, size_t boundary, size_t end) {
        spans_.AddSpan(start, boundary);
        spans_.AddSpan(boundary, end);
    }
    rime::Spans Syllabify(const rime::Phrase *) override { return spans_; }
private:
    rime::Spans spans_;
};

// Recover a schema-provided abbreviation + full syllable hidden by a complete
// single-syllable spelling (e.g. shuo vs s'huo). Only look up real two-syllable
// dictionary words; do not generate sentences or guess arbitrary missing letters.
inline rime::CandidateList mixedAbbreviationPhrases(
    const rime::an<rime::Candidate> &original, const std::string &input) {
    constexpr size_t limit = 8;
    rime::CandidateList result;
    const auto base = std::dynamic_pointer_cast<rime::Phrase>(
        rime::Candidate::GetGenuineCandidate(original));
    if (!base || (base->type() != "phrase" && base->type() != "user_phrase") ||
        !base->is_exact_match() || base->code().size() != 1 ||
        input.size() < 2 || input.size() > 6 ||
        !std::all_of(input.begin(), input.end(), [](char c) { return c >= 'a' && c <= 'z'; }))
        return result;
    const auto found = decodingTranslators.find(base->language());
    if (found == decodingTranslators.end()) return result;  // A different translator kind.
    auto *translator = found->second;
    auto prism = translator->dict()->prism();
    int spelling;
    if (!prism->GetValue(input, &spelling)) return result;
    bool fullSpelling = false;
    for (auto accessor = prism->QuerySpelling(spelling); !accessor.exhausted(); accessor.Next()) {
        const auto properties = accessor.properties();
        fullSpelling |= accessor.syllable_id() == base->code()[0] &&
            properties.type == rime::kNormalSpelling && !properties.is_correction;
    }
    if (!fullSpelling) return result;

    const bool useUserDict = translator->user_dict() && translator->user_dict()->loaded() &&
        !translator->IsUserDictDisabledFor(input);
    for (size_t boundary = 1; boundary < input.size(); ++boundary) {
        int prefix, suffix;
        if (!prism->GetValue(input.substr(0, boundary), &prefix) ||
            !prism->GetValue(input.substr(boundary), &suffix)) continue;
        rime::SyllableGraph graph;
        graph.input_length = graph.interpreted_length = input.size();
        auto &left = graph.edges[0][boundary];
        auto &right = graph.edges[boundary][input.size()];
        for (auto accessor = prism->QuerySpelling(prefix); !accessor.exhausted(); accessor.Next()) {
            const auto properties = accessor.properties();
            if (properties.type != rime::kAbbreviation || properties.is_correction) continue;
            auto &edge = left[accessor.syllable_id()];
            static_cast<rime::SpellingProperties &>(edge) = properties;
            edge.end_pos = boundary;
        }
        for (auto accessor = prism->QuerySpelling(suffix); !accessor.exhausted(); accessor.Next()) {
            const auto properties = accessor.properties();
            if (properties.type > rime::kFuzzySpelling || properties.is_correction) continue;
            auto &edge = right[accessor.syllable_id()];
            static_cast<rime::SpellingProperties &>(edge) = properties;
            edge.end_pos = input.size();
        }
        if (left.empty() || right.empty()) continue;
        graph.vertices[0] = rime::kNormalSpelling;
        graph.vertices[boundary] = graph.vertices[input.size()] = rime::kAbbreviation;
        for (auto &[start, ends] : graph.edges)
            for (auto &[end, spellings] : ends)
                for (auto &[id, properties] : spellings)
                    graph.indices[start][id].push_back(&properties);
        auto syllabifier = rime::New<MixedAbbreviationSyllabifier>(
            base->start(), base->start() + boundary, base->end());
        auto collect = [&](auto entries, const char *type) {
            if (!entries) return;
            const auto complete = entries->find(input.size());
            if (complete == entries->end()) return;
            auto &words = complete->second;
            size_t count = 0;
            while (!words.exhausted() && count++ < limit) {
                const auto entry = words.Peek();
                if (entry->IsExactMatch() && entry->code.size() == 2 &&
                    !translator->blacklist().count(entry->text)) {
                    // Copy before setting presentation fields: dictionary entries
                    // may also be shared by the original translation.
                    auto phrase = rime::New<rime::Phrase>(base->language(), type,
                        base->start(), base->end(), rime::New<rime::DictEntry>(*entry));
                    phrase->set_syllabifier(syllabifier);
                    phrase->set_preedit(translator->FormatPreedit(
                        input.substr(0, boundary) + translator->delimiters().at(0) +
                        input.substr(boundary)));
                    phrase->set_quality(std::exp(entry->weight) + translator->initial_quality() +
                        entry->quality_len / input.size());
                    result.push_back(phrase);
                }
                words.Next();
            }
        };
        if (useUserDict) collect(translator->user_dict()->Lookup(graph, 0, 2), "user_phrase");
        collect(translator->dict()->Lookup(graph, 0, &translator->blacklist(), false), "phrase");
    }
    // Limit only the new pool; the original translation and its leader survive.
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a->quality() > b->quality();
    });
    std::set<std::string> seen{original->text()};
    rime::CandidateList unique;
    for (const auto &candidate : result) {
        if (seen.insert(candidate->text()).second) unique.push_back(candidate);
        if (unique.size() == limit) break;
    }
    return unique;
}
}  // namespace chinese_ime_lm
