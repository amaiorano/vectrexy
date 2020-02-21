#pragma once

#include "core/Base.h"
#include "core/FileSystem.h"
#include "debugger/DebugSymbols.h"

class DapDebugger {
public:
    void OnRomLoaded(const char* file);

private:
    void ParseRst(const fs::path& rstFile);

    DebugSymbols m_debugSymbols;
};
