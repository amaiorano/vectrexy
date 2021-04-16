#include "engine/EngineUtil.h"
#include "core/ConsoleOutput.h"
#include "engine/Paths.h"

bool EngineUtil::FindAndSetRootPath(fs::path startPath) {
    // Look for bios file in current directory and up parent dirs
    // and set current working directory to the one found.
    fs::path biosRomFile = Paths::biosRomFile;

    auto currDir = startPath.remove_filename();

    do {
        if (fs::exists(currDir / biosRomFile)) {
            fs::current_path(currDir);
            // Printf("Root path set to: %s\n", fs::current_path().string().c_str());
            return true;
        }
        currDir = currDir.parent_path();
    } while (!currDir.empty());

    // Errorf("Bios rom file not found: %s", biosRomFile.string().c_str());
    return false;
}
