// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace chinese_ime_lm {
inline uint8_t candidateFacts(size_t start, size_t end, size_t segmentStart,
                              size_t inputEnd, std::string_view originType) {
    const bool coversKey = start == segmentStart && end == inputEnd &&
                           originType != "completion";
    const bool dictionary = originType == "phrase" || originType == "user_phrase" ||
                            originType == "table" || originType == "user_table";
    return (coversKey ? 1 : 0) | (dictionary ? 2 : 0);
}
} // namespace chinese_ime_lm
