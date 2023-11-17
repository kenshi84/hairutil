#include <algorithm>
#include <numeric>
#include <cctype>
#include <vector>
#include <sstream>
#include <string>

namespace util {

template <typename T>
struct StatsInfo {
    T min, max, median;
    double average;
    std::vector<T> largest;
    std::vector<T> smallest;
};

template <typename T, class GetScore>
inline StatsInfo<T> get_stats(std::vector<T>& vec, GetScore get_score, unsigned int sort_size) {
    auto first = vec.begin();
    auto last = vec.end();

    const auto n = vec.size();
    const auto comp_lt = [&](const auto& a, const auto& b) { return get_score(a) < get_score(b); };
    const auto comp_gt = [&](const auto& a, const auto& b) { return get_score(a) > get_score(b); };

    StatsInfo<T> res;

    // Min, max
    const auto [min, max] = std::minmax_element(first, last, comp_lt);
    res.min = *min;
    res.max = *max;

    // Median (no averaging in case of even n)
    std::nth_element(first, first + n / 2, last, comp_lt);
    const auto median = first + n / 2;
    res.median = *median;

    // Average
    res.average = std::accumulate(first, last, 0.0, [&](const double a, const auto& b) { return a + get_score(b); }) / (double)n;

    // Partial sort
    if (sort_size > 0) {
        std::partial_sort(first, first + sort_size, last, comp_gt);
        res.largest = std::vector<T>(first, first + sort_size);
        std::partial_sort(first, first + sort_size, last, comp_lt);
        res.smallest = std::vector<T>(first, first + sort_size);
    }

    return res;
}

inline std::string trim_whitespaces(const std::string& str_in) {
    std::string str_out = str_in;
    str_out.erase(str_out.begin(), std::find_if(str_out.begin(), str_out.end(), [](int ch) { return !std::isspace(ch); }));
    str_out.erase(std::find_if(str_out.rbegin(), str_out.rend(), [](int ch) { return !std::isspace(ch); }).base(), str_out.end());
    return str_out;
}

inline std::string replace_space_with_underscore(const std::string& str_in) {
    std::string str_out = str_in;
    std::replace_if(str_out.begin(), str_out.end(), [](char c) { return std::isspace(c); }, '_');
    return str_out;
}

inline std::string squash_underscores(const std::string& str_in) {
    std::string str_out = str_in;
    auto new_end = std::unique(str_out.begin(), str_out.end(), [](char lhs, char rhs) { return lhs == rhs && lhs == '_'; });
    str_out.erase(new_end, str_out.end());
    return str_out;
}
    
template <typename T>
inline T lexical_cast(const std::string& str) {
    T value;
    std::istringstream(str) >> value;
    return value;
}

template <typename T>
inline std::vector<T> parse_comma_separated_values(const std::string& str) {
    std::vector<T> values;
    std::string::size_type start = 0;
    std::string::size_type end = 0;
    while ((end = str.find(',', start)) != std::string::npos) {
        values.push_back(lexical_cast<T>(str.substr(start, end - start)));
        start = end + 1;
    }
    values.push_back(lexical_cast<T>(str.substr(start)));
    return values;
}

}
