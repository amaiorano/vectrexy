#pragma once

#include <fstream>
#include <string>
#include <vector>

class StabsFile {
public:
    StabsFile() = default;
    StabsFile(std::ifstream& fin);

    bool ReadLine(std::string& line);
    bool PeekNextLine(std::string& line);

    size_t LineNum() const;

private:
    struct Line {
        std::string text;
        size_t lineNum;
    };
    std::vector<Line> m_lines;
    size_t m_currLine = 0;
};
