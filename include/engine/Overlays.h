#pragma once

#include "core/FileSystem.h"
#include <optional>
#include <vector>

class Overlays {
public:
    void LoadOverlays(const fs::path& overlaysDir);
    std::optional<fs::path> FindOverlay(const char* romFile);

private:
    struct OverlayFile {
        fs::path path;
    };
    std::vector<OverlayFile> m_overlayFiles;
};
