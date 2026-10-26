// SPDX-License-Identifier: MIT
#pragma once
#include "keyboard-neighbors.h"
#include "transposition.h"
#include <rime/dict/corrector.h>

namespace chinese_ime_lm {
class TypingCorrector final : public rime::Corrector {
public:
    TypingCorrector(bool transpositions, bool neighbors)
        : transpositions_(transpositions), neighbors_(neighbors) {}
    void ToleranceSearch(const rime::Prism &prism, const std::string &key,
                         rime::corrector::Corrections *results,
                         size_t tolerance) override {
        // Keep upstream's one-step same-row search. Rime assigns a fixed penalty
        // regardless of distance, so unrestricted five-edit search is unsuitable.
        near_.ToleranceSearch(prism, key, results, std::min<size_t>(tolerance, 1));
        if (tolerance == 0) return;
        if (neighbors_) {
            std::array<bool, 6> accepted{};
            for (size_t length = 1; length <= std::min<size_t>(key.size(), 6); ++length) {
                int original;
                accepted[length - 1] = prism.GetValue(key.substr(0, length), &original);
            }
            chinese_ime_lm::keyboardPinyinPrefixes(key, std::min<size_t>(tolerance, 2),
            [&](const std::string &corrected, size_t cost) {
                size_t node = 0, position = 0;
                const int spelling = prism.trie().traverse(
                    corrected.data(), node, position, corrected.size());
                if (spelling == -2) return false;
                // Preserve accepted spellings and user aliases. For example,
                // valid ming must not gain jing solely from the keyboard layout.
                bool anchored = false;
                for (size_t i = 0; i < corrected.size(); ++i)
                    anchored |= corrected[i] == key[i];
                // Two-step guesses need an unchanged letter as evidence. In
                // particular, replacing BOTH letters of a two-letter input
                // creates unrelated words through abbreviation paths.
                if (spelling >= 0 && cost > 0 && (cost == 1 || anchored) &&
                    !accepted[corrected.size() - 1])
                    results->Alter(spelling, {cost, spelling, corrected.size()});
                return true;
            });
        }
        if (transpositions_) chinese_ime_lm::transposedPinyinPrefixes(key,
            [&](const std::string &corrected, size_t length) {
                int spelling;
                if (prism.GetValue(corrected, &spelling)) {
                    results->Alter(spelling, {1, spelling, length});
                }
            });
    }
private:
    bool transpositions_, neighbors_;
    rime::NearSearchCorrector near_;
};

}  // namespace chinese_ime_lm
