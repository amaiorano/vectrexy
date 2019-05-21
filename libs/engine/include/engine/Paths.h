#pragma once

#include "core/FileSystem.h"

namespace Paths {
    inline fs::path dataDir = "data";

    inline fs::path overlaysDir = dataDir / "overlays";
    inline fs::path romsDir = dataDir / "roms";
    inline fs::path userDir = dataDir / "user";
    inline fs::path biosDir = dataDir / "bios";
    inline fs::path devDir = dataDir / "dev";

    inline fs::path biosRomFile = biosDir / "bios_rom.bin";
    inline fs::path optionsFile = userDir / "options.txt";
    inline fs::path imguiIniFile = userDir / "imgui.ini";
} // namespace Paths
