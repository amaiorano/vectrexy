#pragma once

#include <fstream>
#include <string>
#include <vector>

class RstFile {
public:
    RstFile() = default;
    RstFile(std::ifstream& fin);

    bool ReadLine(std::string& line);
    bool PeekNextLine(std::string& line);

private:
    std::vector<std::string> m_lines;
    size_t m_currLine = 0;
};
