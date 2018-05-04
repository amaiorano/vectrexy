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
    // Returns a confidence ratio in [0.0, 1.0] from no match to perfect match
    float OverlayFileFuzzyMatch(const fs::path& p1, const fs::path& p2);

    struct OverlayFile {
        fs::path path;
    };
    std::vector<OverlayFile> m_overlayFiles;
};
