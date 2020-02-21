#pragma once

#include "core/FileSystem.h"

class DebugSymbols;

class RstParser {
public:
    RstParser(DebugSymbols& debugSymbols);
    bool Parse(const fs::path& rstFile);

private:
    DebugSymbols* m_debugSymbols{};
};
