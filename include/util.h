#include <algorithm>
#include <cctype>
#include <vector>
#include <sstream>
#include <string>

namespace util {

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
