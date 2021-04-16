#include "debugger/RstFile.h"
#include "core/Base.h"
#include "debugger/RstMatchers.h"

namespace {
    bool GetLine(std::ifstream& fin, std::string& line) {
        if (!std::getline(fin, line))
            return false;

        // stab string lines that are too long are extended over multiple consecutive lines by
        // adding a '\\' token at the end of the 'string' portion, for example:
        //   59;.stabs	"WeekDay:t25=eMonday:0,Tuesday:1,Wednesday:2,EndOfDays:2,\\", 128, 0, 0, 0
        //   60;.stabs	"Foo:-5000,;", 128, 0, 0, 0
        // To simplify parsing, we join all these lines into a single one.
        if (auto stabs = StabStringMatch(line)) {
            const char* LineContinuationToken = R"(\\)";
            auto s = stabs.String();

            while (s.size() >= 2 && s.substr(s.size() - 2) == LineContinuationToken) {
                std::string nextLine;
                if (!std::getline(fin, nextLine))
                    return false;
                auto nextStabs = StabStringMatch(nextLine);
                ASSERT(nextStabs);
                s = nextStabs.String();

                // Replace the "\\" with the string contents of the next line
                auto index = line.rfind(LineContinuationToken);
                ASSERT(index != std::string::npos);
                line.erase(index, 2);
                line.insert(index, s);
            }
        }

        return true;
    }
} // namespace

RstFile::RstFile(std::ifstream& fin) {
    std::string line;
    while (GetLine(fin, line)) {
        m_lines.push_back(std::move(line));
    }
}

bool RstFile::ReadLine(std::string& line) {
    if (!PeekNextLine(line))
        return false;
    ++m_currLine;
    return true;
}

bool RstFile::PeekNextLine(std::string& line) {
    if (m_currLine >= m_lines.size())
        return false;
    line = m_lines[m_currLine];
    return true;
}
