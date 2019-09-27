#pragma once

#include "core/FileSystem.h"
#include <optional>

namespace EngineUtil {
    // Look for bios file in startPath's directory and up parent dirs
    // and set current working directory to the one found.
    bool FindAndSetRootPath(fs::path startPath);
} // namespace EngineUtil
