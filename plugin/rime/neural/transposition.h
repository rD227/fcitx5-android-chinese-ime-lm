// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace chinese_ime_lm {
// Full pinyin syllables have at most six letters (e.g. zhuang, shuang).
// Swap each position once in a bounded input copy. The visitor searches all
// dictionary prefixes in one trie traversal, accepting lengths >= minimumLength
// so both exchanged letters belong to the syllable. Delimiters stop the copy.
template <typename Visit>
void transposedPinyinKeys(std::string_view input, Visit visit) {
    size_t limit = 0;
    while (limit < std::min<size_t>(input.size(), 6) &&
           input[limit] >= 'a' && input[limit] <= 'z') ++limit;
    std::string corrected(input.substr(0, limit));
    for (size_t position = 0; position + 1 < limit; ++position) {
        if (corrected[position] == corrected[position + 1]) continue;
        std::swap(corrected[position], corrected[position + 1]);
        visit(corrected, position + 2);
        std::swap(corrected[position], corrected[position + 1]);
    }
}
} // namespace chinese_ime_lm
