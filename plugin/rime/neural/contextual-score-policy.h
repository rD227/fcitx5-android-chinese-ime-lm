// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace chinese_ime_lm {
// Promote only when left context strengthens the alternative's advantage over
// the leader. A generally frequent word must not win solely on its prior.
inline size_t contextualBest(const std::vector<float> &conditioned,
                             const std::vector<float> &unconditioned,
                             const std::vector<uint8_t> &facts) {
    size_t selected = 0;
    for (size_t index = 1; index < facts.size(); ++index) {
        if (!(facts[index] & 1) || conditioned[index] <= conditioned[selected]) continue;
        const float advantage = conditioned[index] - conditioned[0];
        const float priorAdvantage = unconditioned[index] - unconditioned[0];
        if (advantage > priorAdvantage) selected = index;
    }
    return selected;
}
} // namespace chinese_ime_lm
