#pragma once

#include <filesystem>
#include <optional>
#include <vector>

//@TODO: should be std::filesystem
namespace fs = std::experimental::filesystem;

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
