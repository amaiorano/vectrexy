#include "debugger/DapDebugger.h"
#include "debugger/RstParser.h"

#include <memory>

void DapDebugger::OnRomLoaded(const char* file) {
    // Collect .rst files in the same folder as the rom file
    fs::path rstDir = fs::path{file}.remove_filename();

    std::vector<fs::path> rstFiles;
    for (auto& d : fs::directory_iterator(rstDir)) {
        if (d.path().extension() == ".rst") {
            rstFiles.push_back(d.path());
        }
    }

    for (auto& rstFile : rstFiles) {
        ParseRst(rstFile);
    }
}

void DapDebugger::ParseRst(const fs::path& rstFile) {
    RstParser parser{m_debugSymbols};
    parser.Parse(rstFile);
}
