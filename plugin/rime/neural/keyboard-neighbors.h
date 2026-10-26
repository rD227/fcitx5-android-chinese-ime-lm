// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace chinese_ime_lm {
inline bool neighboringKeys(char a, char b) {
    constexpr std::string_view rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    constexpr double offsets[] = {0.0, 0.25, 0.75};
    int rowA = -1, rowB = -1;
    double xA = 0, xB = 0;
    for (int row = 0; row < 3; ++row) {
        const auto first = rows[row].find(a), second = rows[row].find(b);
        if (first != std::string_view::npos) { rowA = row; xA = first + offsets[row]; }
        if (second != std::string_view::npos) { rowB = row; xB = second + offsets[row]; }
    }
    return a != b && rowA >= 0 && rowB >= 0 &&
        std::abs(rowA - rowB) <= 1 && std::abs(xA - xB) <= 1.0;
}

// Search only needs distances up to two. Three means outside that budget.
inline size_t keyboardSteps(char a, char b) {
    if (a < 'a' || a > 'z' || b < 'a' || b > 'z') return 3;
    static const auto distances = [] {
        std::array<std::array<size_t, 26>, 26> result{};
        for (size_t i = 0; i < 26; ++i)
            for (size_t j = 0; j < 26; ++j)
                result[i][j] = i == j ? 0 : neighboringKeys('a' + i, 'a' + j) ? 1 : 3;
        // Only direct edges can form a two-step path; do not propagate longer
        // distances through a partially updated matrix.
        const auto direct = result;
        for (size_t i = 0; i < 26; ++i)
            for (size_t j = 0; j < 26; ++j)
                for (size_t via = 0; via < 26; ++via)
                    if (direct[i][via] == 1 && direct[via][j] == 1)
                        result[i][j] = std::min<size_t>(result[i][j], 2);
        return result;
    }();
    return distances[a - 'a'][b - 'a'];
}

// Visit each possible prefix with its total keyboard cost. The visitor returns
// false when the dictionary trie rejects a prefix, pruning all its extensions.
// Cost two admits two adjacent mistakes or one key two steps away. No word or
// syllable replacement table is used. Apostrophes and other nonletters stop it.
template <typename Visit>
void keyboardPinyinPrefixes(std::string_view input, size_t budget, Visit visit) {
    size_t limit = 0;
    while (limit < std::min<size_t>(input.size(), 6) &&
           input[limit] >= 'a' && input[limit] <= 'z') ++limit;
    std::string prefix;
    auto search = [&](auto &&self, size_t cost) -> void {
        if (prefix.size() == limit) return;
        const char original = input[prefix.size()];
        for (char alternative = 'a'; alternative <= 'z'; ++alternative) {
            const size_t steps = keyboardSteps(original, alternative);
            if (steps > 2 || cost + steps > budget) continue;
            prefix.push_back(alternative);
            if (visit(prefix, cost + steps)) self(self, cost + steps);
            prefix.pop_back();
        }
    };
    search(search, 0);
}
}  // namespace chinese_ime_lm
