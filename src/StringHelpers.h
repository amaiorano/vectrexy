#pragma once

#include <string>
#include <vector>

inline std::vector<std::string> Split(const std::string& s, const char* delimiters) {
    std::vector<std::string> result;
    size_t startIndex = 0;
    while ((startIndex = s.find_first_not_of(delimiters, startIndex)) != std::string::npos) {
        size_t endIndex = s.find_first_of(delimiters, startIndex + 1);

        if (endIndex == std::string::npos) {
            result.emplace_back(s.substr(startIndex));
            break;
        } else {
            result.emplace_back(s.substr(startIndex, endIndex - startIndex));
            startIndex = endIndex;
        }
    }
    return result;
}

template <typename StringContainer>
std::string Join(const StringContainer& values, const char* between) {
    std::string result;
    for (auto iter = std::begin(values); iter != std::end(values); ++iter) {
        result += *iter;
        if (std::distance(iter, std::end(values)) > 1)
            result += between;
    }
    return result;
}

inline std::string Trim(std::string s, const char* delimiters = " \t") {
    size_t index = s.find_first_not_of(delimiters, 0);
    if (index != std::string::npos) {
        s = s.substr(index);
    }
    index = s.find_last_not_of(delimiters);
    if (index != std::string::npos) {
        s = s.substr(0, index + 1);
    }
    return s;
}

template <typename StringContainer>
inline StringContainer Trim(StringContainer values) {
    std::transform(begin(values), end(values), begin(values),
                   [](const auto& s) { return Trim(s); });
    return values;
}
