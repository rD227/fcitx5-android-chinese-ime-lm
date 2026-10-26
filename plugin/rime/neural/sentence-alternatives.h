// SPDX-License-Identifier: MIT
#pragma once
#include <rime/gear/translator_commons.h>
#include <set>

namespace chinese_ime_lm {
// Reuse the original syllabifier, language, and dictionary entries. Replacing
// whole component prefixes keeps selection, cursor spans and learning attached
// to the same input; replacing display text alone would violate that contract.
class AlternativeSentence final : public rime::Sentence {
public:
    AlternativeSentence(const rime::Sentence &base, const rime::Phrase &prefix,
                        size_t replaced) : Sentence(base) {
        entry_ = rime::New<rime::DictEntry>();
        components_.clear();
        word_lengths_.clear();
        set_start(0);
        set_end(0);
        size_t end = prefix.end() - base.start();
        Extend(prefix.entry(), end, base.weight());
        for (size_t i = replaced; i < base.components().size(); ++i) {
            end += base.word_lengths()[i];
            Extend(base.components()[i], end, base.weight());
        }
        Offset(base.start());
        set_preedit(base.preedit());
    }
};

inline void appendSentenceAlternatives(rime::CandidateList &candidates,
                                      size_t limit) {
    if (candidates.empty()) return;
    const auto base = std::dynamic_pointer_cast<rime::Sentence>(
        rime::Candidate::GetGenuineCandidate(candidates.front()));
    if (!base) return;
    std::set<std::string> seen;
    for (const auto &candidate : candidates) seen.insert(candidate->text());
    const size_t originalCount = candidates.size();
    size_t added = 0;
    for (size_t i = 1; i < originalCount && added < limit; ++i) {
        const auto prefix = std::dynamic_pointer_cast<rime::Phrase>(
            rime::Candidate::GetGenuineCandidate(candidates[i]));
        if (!prefix || prefix->language() != base->language() ||
            prefix->start() != base->start() || prefix->end() <= base->start() ||
            prefix->end() >= base->end() || !prefix->is_exact_match() ||
            prefix->type() == "completion") continue;
        size_t end = base->start();
        size_t replaced = 0;
        rime::Code code;
        while (replaced < base->word_lengths().size() && end < prefix->end()) {
            const auto &component = base->components()[replaced];
            code.insert(code.end(), component.code.begin(), component.code.end());
            end += base->word_lengths()[replaced++];
        }
        // Reuse only the same decoded pinyin path. Another spelling correction
        // or syllable split needs its own decoder evidence, not a borrowed tail.
        if (end != prefix->end() || code != prefix->entry().code) continue;
        auto alternative = rime::New<AlternativeSentence>(*base, *prefix, replaced);
        if (seen.insert(alternative->text()).second) {
            candidates.push_back(std::move(alternative));
            ++added;
        }
    }
}
} // namespace chinese_ime_lm
