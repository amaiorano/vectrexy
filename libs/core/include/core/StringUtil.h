#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace StringUtil {

    enum class KeepEmptyEntries { False, True };

    inline std::vector<std::string>
    Split(const std::string& s, const char* delimiters,
          KeepEmptyEntries KeepEmptyEntries = KeepEmptyEntries::False) {
        if (s.empty())
            return {};

        std::string delims = delimiters;
        std::vector<std::string> result;
        std::string curr = "";
        const size_t lastIndex = s.length() - 1;
        size_t startIndex = 0;
        size_t i = startIndex;

        while (i <= lastIndex) {
            if (delims.find(s[i]) != -1) {
                auto token = s.substr(startIndex, i - startIndex);
                if (!token.empty() || KeepEmptyEntries == KeepEmptyEntries::True)
                    result.push_back(token);
                startIndex = i + 1;
            } else if (i == lastIndex) {
                auto token = s.substr(startIndex, i - startIndex + 1);
                if (!token.empty() || KeepEmptyEntries == KeepEmptyEntries::True)
                    result.push_back(token);
            }
            ++i;
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

    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](char c) { return (char)::tolower(c); });
        return s;
    }

    inline std::string Remove(std::string_view s, std::string_view toRemove) {
        std::string result;
        result.reserve(s.size());
        size_t offset = 0;
        size_t index{};
        while ((index = s.find_first_of(toRemove, offset)) != std::string::npos) {
            result += s.substr(offset, index - offset);
            offset = index + toRemove.size();
        }
        result += s.substr(offset, s.size() - offset);
        return result;
    }

    inline std::string Replace(std::string_view s, std::string_view toReplace,
                               std::string_view replaceWith) {
        std::string result;
        result.reserve(s.size());
        size_t offset = 0;
        size_t index{};
        while ((index = s.find_first_of(toReplace, offset)) != std::string::npos) {
            result += s.substr(offset, index - offset);
            result += replaceWith;
            offset = index + toReplace.size();
        }
        result += s.substr(offset, s.size() - offset);
        return result;
    }

} // namespace StringUtil
