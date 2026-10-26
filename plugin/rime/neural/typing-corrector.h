// SPDX-License-Identifier: MIT
#pragma once
#include "keyboard-neighbors.h"
#include "transposition.h"
#include <rime/dict/corrector.h>

namespace chinese_ime_lm {
struct TypingCorrectionOptions {
    bool native = true;
    bool neighbors = false;
    bool multiple = true;
    bool distant = true;
    bool transpositions = false;
};

class TypingCorrector final : public rime::Corrector {
public:
    explicit TypingCorrector(TypingCorrectionOptions options) : options_(options) {}
    void ToleranceSearch(const rime::Prism &prism, const std::string &key,
                         rime::corrector::Corrections *results,
                         size_t tolerance) override {
        // Keep upstream's one-step same-row search. Rime assigns a fixed penalty
        // regardless of distance, so unrestricted five-edit search is unsuitable.
        if (options_.native)
            near_.ToleranceSearch(prism, key, results, std::min<size_t>(tolerance, 1));
        if (tolerance == 0) return;
        if (options_.neighbors) {
            std::array<bool, 6> accepted{};
            for (size_t length = 1; length <= std::min<size_t>(key.size(), 6); ++length) {
                int original;
                accepted[length - 1] = prism.GetValue(key.substr(0, length), &original);
            }
            const size_t budget = options_.multiple || options_.distant ? 2 : 1;
            chinese_ime_lm::keyboardPinyinPrefixes(key, std::min(tolerance, budget),
            [&](const std::string &corrected, size_t cost) {
                size_t node = 0, position = 0;
                const int spelling = prism.trie().traverse(
                    corrected.data(), node, position, corrected.size());
                if (spelling == -2) return false;
                // Preserve accepted spellings and user aliases. For example,
                // valid ming must not gain jing solely from the keyboard layout.
                size_t substitutions = 0;
                for (size_t i = 0; i < corrected.size(); ++i)
                    substitutions += corrected[i] != key[i];
                // Two-step guesses need an unchanged letter as evidence. In
                // particular, replacing BOTH letters of a two-letter input
                // creates unrelated words through abbreviation paths.
                const bool enabled = cost == 1 || (substitutions < corrected.size() &&
                    ((substitutions == 1 && options_.distant) ||
                     (substitutions == 2 && options_.multiple)));
                if (spelling >= 0 && cost > 0 && enabled &&
                    !accepted[corrected.size() - 1])
                    results->Alter(spelling, {cost, spelling, corrected.size()});
                return true;
            });
        }
        if (options_.transpositions) chinese_ime_lm::transposedPinyinKeys(key,
            [&](const std::string &corrected, size_t minimumLength) {
                // One trie traversal validates every prefix for this exchange.
                std::array<rime::Prism::Match, 6> matches;
                const size_t count = prism.trie().commonPrefixSearch(
                    corrected.data(), matches.data(), matches.size(), corrected.size());
                for (size_t i = 0; i < count; ++i)
                    if (matches[i].length >= minimumLength)
                        results->Alter(matches[i].value, {1, matches[i].value, matches[i].length});
            });
    }
private:
    TypingCorrectionOptions options_;
    rime::NearSearchCorrector near_;
};

}  // namespace chinese_ime_lm
