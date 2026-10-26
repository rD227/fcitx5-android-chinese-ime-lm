// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace chinese_ime_lm {
// Full pinyin syllables have at most six letters (e.g. zhuang, shuang).
// Enumerate possible syllable prefixes without changing the original input.
// Explicit delimiters stop the search; the Rime prism validates each spelling.
template <typename Visit>
void transposedPinyinPrefixes(std::string_view input, Visit visit) {
    const size_t limit = std::min<size_t>(input.size(), 6);
    for (size_t length = 2; length <= limit; ++length) {
        const auto key = input.substr(0, length);
        if (!std::all_of(key.begin(), key.end(), [](char c) {
                return c >= 'a' && c <= 'z';
            })) break;
        std::string corrected(key);
        for (size_t position = 0; position + 1 < length; ++position) {
            if (corrected[position] == corrected[position + 1]) continue;
            std::swap(corrected[position], corrected[position + 1]);
            visit(corrected, length);
            std::swap(corrected[position], corrected[position + 1]);
        }
    }
}
} // namespace chinese_ime_lm
