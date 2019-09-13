#pragma once

#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>

namespace RegexUtil {

    // RegexReplace is similar to std::regex_replace, except it allows you to provide a callback
    // function that will get called for each match, and from which you return the replacement
    // string. From https://stackoverflow.com/a/37516316

    template <class BidirIt, class Traits, class CharT, class UnaryFunction>
    std::basic_string<CharT> RegexReplace(BidirIt first, BidirIt last,
                                          const std::basic_regex<CharT, Traits>& re,
                                          UnaryFunction f) {
        std::basic_string<CharT> s;

        typename std::match_results<BidirIt>::difference_type positionOfLastMatch = 0;
        auto endOfLastMatch = first;

        auto callback = [&](const std::match_results<BidirIt>& match) {
            auto positionOfThisMatch = match.position(0);
            auto diff = positionOfThisMatch - positionOfLastMatch;

            auto startOfThisMatch = endOfLastMatch;
            std::advance(startOfThisMatch, diff);

            s.append(endOfLastMatch, startOfThisMatch);
            s.append(f(match));

            auto lengthOfMatch = match.length(0);

            positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

            endOfLastMatch = startOfThisMatch;
            std::advance(endOfLastMatch, lengthOfMatch);
        };

        std::sregex_iterator begin(first, last, re), end;
        std::for_each(begin, end, callback);

        s.append(endOfLastMatch, last);

        return s;
    }

    template <class Traits, class CharT, class UnaryFunction>
    std::string RegexReplace(std::string_view s, const std::basic_regex<CharT, Traits>& re,
                             UnaryFunction f) {
        auto sc = std::string{s}; // TODO: get rid of this copy
        return RegexReplace(sc.cbegin(), sc.cend(), re, f);
    }

} // namespace RegexUtil
