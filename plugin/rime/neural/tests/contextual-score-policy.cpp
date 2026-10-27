// SPDX-License-Identifier: MIT
#include "../contextual-score-policy.h"
#include <cassert>

int main() {
    // Measured with the released 4M weights: [派对, 排队]. The word prior
    // already favors 排队; only 还在 strengthens that preference.
    const std::vector<float> prior{-6.00464f, -5.27935f};
    assert(chinese_ime_lm::contextualBest({-6.11978f, -4.27005f}, prior, {1, 1}) == 1);
    assert(chinese_ime_lm::contextualBest({-5.872f, -5.42753f}, prior, {1, 1}) == 0);
    assert(chinese_ime_lm::contextualBest({-5.44313f, -5.23212f}, prior, {1, 1}) == 0);
    // A different spelling path remains ineligible even when the model likes it.
    assert(chinese_ime_lm::contextualBest({-6.11978f, -4.27005f}, prior, {1, 0}) == 0);
}
