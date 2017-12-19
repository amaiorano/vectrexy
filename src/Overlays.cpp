#include "Overlays.h"
#include "StringHelpers.h"

void Overlays::LoadOverlays() {
    for (auto& dir : fs::directory_iterator("overlays")) {
        if (dir.path().extension() == ".png") {
            m_overlayFiles.push_back({fs::absolute(dir.path())});
        }
    }
}

std::optional<fs::path> Overlays::FindOverlay(const char* romFile) {
    std::optional<fs::path> bestMatch{};
    int bestConfidence = 0;
    for (auto& overlayFile : m_overlayFiles) {
        int c = OverlayFileFuzzyMatch(romFile, overlayFile.path);
        if (c > bestConfidence) {
            bestConfidence = c;
            bestMatch = overlayFile.path;
        }
    }
    return bestMatch;
}

int Overlays::OverlayFileFuzzyMatch(const fs::path& p1, const fs::path& p2) {
    auto TrimFileName = [](const std::string& s) {
        auto index = s.find(" by GCE");
        if (index != std::string::npos) {
            return s.substr(0, index);
        }
        index = s.find(" (");
        if (index != std::string::npos) {
            return s.substr(0, index);
        }
        return s;
    };

    auto s1 = Split(TrimFileName(p1.filename().string()), " ");
    auto s2 = Split(TrimFileName(p2.filename().string()), " ");

    int matchedParts = 0;
    for (size_t i = 0; i < std::min(s1.size(), s2.size()); ++i) {
        if (s1[i] != s2[i])
            break;
        ++matchedParts;
    }
    return matchedParts;
}
