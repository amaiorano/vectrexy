#include "stabs_parser/StabsFile.h"
#include "core/Base.h"
#include <regex>

namespace {
    const std::regex StabsStringRe(
        R"(.*\.stabs[[:space:]]*\"(.*)\",[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

    bool GetLine(std::istream& in, std::string& line, size_t& linesRead) {
        if (!std::getline(in, line))
            return false;

        linesRead = 1;

        // stab string lines that are too long are extended over multiple consecutive lines by
        // adding a '\\' token at the end of the 'string' portion, for example:
        //   59;.stabs	"WeekDay:t25=eMonday:0,Tuesday:1,Wednesday:2,EndOfDays:2,\\", 128, 0, 0, 0
        //   60;.stabs	"Foo:-5000,;", 128, 0, 0, 0
        // To simplify parsing, we join all these lines into a single one.
        std::smatch match;
        if (std::regex_match(line, match, StabsStringRe)) {
            const char* LineContinuationToken = R"(\\)";
            auto s = match[1].str();
            while (s.size() >= 2 && s.substr(s.size() - 2) == LineContinuationToken) {
                std::string nextLine;
                if (!std::getline(in, nextLine))
                    return false;

                bool nextMatched = std::regex_match(nextLine, match, StabsStringRe);
                ASSERT(nextMatched);
                s = match[1].str();

                // Replace the "\\" with the string contents of the next line
                auto index = line.rfind(LineContinuationToken);
                ASSERT(index != std::string::npos);
                line.erase(index, 2);
                line.insert(index, s);
                ++linesRead;
            }
        }

        return true;
    }
} // namespace

StabsFile::StabsFile(std::istream& in) {
    size_t lineNum = 1;

    std::string line;
    size_t linesRead;
    while (GetLine(in, line, linesRead)) {
        m_lines.push_back({std::move(line), lineNum});
        lineNum += linesRead;
    }
}

bool StabsFile::ReadLine(std::string& line) {
    if (!PeekNextLine(line))
        return false;
    ++m_currLine;
    return true;
}

bool StabsFile::PeekNextLine(std::string& line) {
    if (m_currLine >= m_lines.size())
        return false;
    line = m_lines[m_currLine].text;
    return true;
}

size_t StabsFile::LineNum() const {
    return m_currLine == 0 ? size_t{1} : m_lines[m_currLine - 1].lineNum;
}
