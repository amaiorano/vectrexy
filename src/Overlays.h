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
    // Returns a confidence level; 0 means no match, 1..n is increasing confidence score
    int OverlayFileFuzzyMatch(const fs::path& p1, const fs::path& p2);

    struct OverlayFile {
        fs::path path;
    };
    std::vector<OverlayFile> m_overlayFiles;
};
