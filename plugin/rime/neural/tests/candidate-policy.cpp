// SPDX-License-Identifier: GPL-3.0-or-later
#include "candidate-policy.h"
#include <cassert>

int main() {
    using chinese_ime_lm::candidateFacts;
    // xian -> 现 / 西安: both cover the same key despite different text lengths.
    assert(candidateFacts(0, 4, 0, 4, "sentence") == 1);
    assert(candidateFacts(0, 4, 0, 4, "phrase") == 3);
    // A prefix (ni in nihao) must never compete with a full-key sentence.
    assert((candidateFacts(0, 2, 0, 5, "phrase") & 1) == 0);
    // Prediction can consume the key but append letters never entered by the user.
    assert((candidateFacts(0, 4, 0, 4, "completion") & 1) == 0);
    // The next unselected segment may start after an already selected prefix.
    assert(candidateFacts(2, 7, 2, 7, "sentence") == 1);
    assert((candidateFacts(0, 7, 2, 7, "sentence") & 1) == 0);
}
