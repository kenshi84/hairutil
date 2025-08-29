#include <gtest/gtest.h>

#include "util.h"

TEST(util_trim_whitespaces, space) {
    const std::string str_in = "  a b c  ";
    const std::string str_out = util::trim_whitespaces(str_in);
    EXPECT_EQ(str_out.compare("a b c"), 0);
}

TEST(util_trim_whitespaces, tab) {
    const std::string str_in = "\t\ta\tb\tc\t\t";
    const std::string str_out = util::trim_whitespaces(str_in);
    EXPECT_EQ(str_out.compare("a\tb\tc"), 0);
}

TEST(util_replace_space_with_underscore, space) {
    const std::string str_in = "a b c";
    const std::string str_out = util::replace_space_with_underscore(str_in);
    EXPECT_EQ(str_out.compare("a_b_c"), 0);
}

TEST(util_replace_space_with_underscore, tab) {
    const std::string str_in = "a\tb\tc";
    const std::string str_out = util::replace_space_with_underscore(str_in);
    EXPECT_EQ(str_out.compare("a_b_c"), 0);
}

TEST(util_replace_space_with_underscore, newline) {
    const std::string str_in = "a\nb\nc";
    const std::string str_out = util::replace_space_with_underscore(str_in);
    EXPECT_EQ(str_out.compare("a_b_c"), 0);
}

TEST(util_squalsh_underscores, test) {
    const std::string str_in = "a__b___c";
    const std::string str_out = util::squash_underscores(str_in);
    EXPECT_EQ(str_out.compare("a_b_c"), 0);
}

TEST(util_lexical_cast, int) {
    const std::string str = "123";
    const int value = util::lexical_cast<int>(str);
    EXPECT_EQ(value, 123);
}

TEST(util_lexical_cast, float) {
    const std::string str = "1.23";
    const float value = util::lexical_cast<float>(str);
    EXPECT_FLOAT_EQ(value, 1.23f);
}

TEST(util_parse_comma_separated_values, int) {
    const std::string str = "1,2,3";
    const std::vector<int> values = util::parse_comma_separated_values<int>(str);
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST(util_parse_comma_separated_values, float) {
    const std::string str = "1.1,2.2,3.3";
    const std::vector<float> values = util::parse_comma_separated_values<float>(str);
    EXPECT_EQ(values.size(), 3);
    EXPECT_FLOAT_EQ(values[0], 1.1f);
    EXPECT_FLOAT_EQ(values[1], 2.2f);
    EXPECT_FLOAT_EQ(values[2], 3.3f);
}
