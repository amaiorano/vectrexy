#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

class Overlays {
public:
    void LoadOverlays();
    std::optional<fs::path> FindOverlay(const char* romFile);

private:
    struct OverlayFile {
        fs::path path;
    };
    std::vector<OverlayFile> m_overlayFiles;
};
