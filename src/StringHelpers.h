#pragma once

#include <string>
#include <vector>

std::vector<std::string> Split(const std::string& s, const char* delimiters) {
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
