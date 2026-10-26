// SPDX-License-Identifier: MIT
#include "../transposition.h"
#include "../keyboard-neighbors.h"
#include <cassert>
#include <map>
#include <set>

int main() {
    for (const auto &[a, b] : {std::pair{'n', 'm'}, {'y', 'u'}, {'i', 'o'},
                              {'n', 'j'}, {'u', 'j'}, {'x', 's'}}) {
        assert(chinese_ime_lm::neighboringKeys(a, b));
        assert(chinese_ime_lm::neighboringKeys(b, a));
    }
    assert(!chinese_ime_lm::neighboringKeys('q', 'm'));
    assert(!chinese_ime_lm::neighboringKeys('n', 'n'));
    assert(!chinese_ime_lm::neighboringKeys('\'', 'n'));
    std::set<std::pair<std::string, size_t>> neighbors;
    chinese_ime_lm::keyboardPinyinPrefixes("ji'hao", 1,
        [&](const auto &key, size_t cost) {
            if (cost) neighbors.emplace(key, key.size());
            return true;
        });
    assert(neighbors.count({"ni", 2}));
    for (const auto &[key, length] : neighbors)
        assert(length <= 2 && key.find('\'') == std::string::npos);
    assert(chinese_ime_lm::keyboardSteps('e', 'r') == 1);
    assert(chinese_ime_lm::keyboardSteps('v', 'x') == 2);
    assert(chinese_ime_lm::keyboardSteps('q', 'm') == 3);
    const std::set<std::string> spellings{"nei", "mei", "sheng", "xi", "xie", "xue"};
    auto keyboardVariants = [&](std::string_view input, size_t budget) {
        std::map<std::string, size_t> found;
        chinese_ime_lm::keyboardPinyinPrefixes(input, budget,
            [&](const std::string &key, size_t cost) {
                const auto next = spellings.lower_bound(key);
                if (next == spellings.end() || next->compare(0, key.size(), key)) return false;
                if (*next == key) found.emplace(key, cost);
                return true;
            });
        return found;
    };
    assert(keyboardVariants("nri", 1).at("nei") == 1);
    assert(keyboardVariants("mro", 1).count("mei") == 0);
    assert(keyboardVariants("mro", 2).at("mei") == 2);
    assert(keyboardVariants("cue", 2).at("xie") == 2);
    assert(keyboardVariants("cue", 2).at("xue") == 1);
    assert(keyboardVariants("shwbg", 2).at("sheng") == 2);
    assert(keyboardVariants("vi", 2).at("xi") == 2);
    assert(keyboardVariants("nr'i", 2).count("nei") == 0);
    assert(keyboardVariants("mr o", 2).count("mei") == 0);
    auto variants = [](const std::string &input) {
        std::set<std::pair<std::string, size_t>> result;
        chinese_ime_lm::transposedPinyinKeys(input,
            [&](const std::string &key, size_t minimumLength) {
                for (size_t consumed = minimumLength; consumed <= key.size(); ++consumed)
                    result.emplace(key.substr(0, consumed), consumed);
            });
        return result;
    };
    assert(variants("tainjian").count({"tian", 4}));
    assert(variants("zhognwen").count({"zhong", 5}));
    assert(variants("wne").count({"wen", 3}));
    assert(variants("shuagn").count({"shuang", 6}));
    assert(variants("suhang").count({"shuang", 6}));
    size_t searches = 0;
    chinese_ime_lm::transposedPinyinKeys("shuang", [&](const auto &, size_t) { ++searches; });
    assert(searches == 5);  // Previously 15 prefix searches, now one per position.
    // An explicit pinyin separator must never be crossed by an exchange.
    const auto separated = variants("shuan'g");
    assert(separated.count({"shuna", 5}));
    for (const auto &[key, consumed] : separated) {
        assert(consumed <= 5 && key.find('\'') == std::string::npos);
    }
    assert(variants("a").empty());
    assert(variants("aa").empty());
    assert(variants("'tain").empty());
    for (const auto &[key, consumed] : variants("zhuangyuan")) {
        assert(consumed <= 6 && key.size() == consumed);
    }
}
