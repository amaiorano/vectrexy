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
    float bestConfidence = 0;
    for (auto& overlayFile : m_overlayFiles) {
        float c = OverlayFileFuzzyMatch(romFile, overlayFile.path);
        if (c > bestConfidence) {
            bestConfidence = c;
            bestMatch = overlayFile.path;
        }
    }

    fflush(stdout);

    if (bestConfidence > 0.5f)
        return bestMatch;
    return {};
}

float Overlays::OverlayFileFuzzyMatch(const fs::path& p1, const fs::path& p2) {
    auto TrimFileName = [](std::string s) {
        s = Join(Split(s, "-"), " "); // Replace dashes with space
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

    size_t matchedParts = 0;
    for (size_t i = 0; i < std::min(s1.size(), s2.size()); ++i) {
        if (_stricmp(s1[i].c_str(), s2[i].c_str()) != 0)
            break;
        ++matchedParts;
    }
    return static_cast<float>(matchedParts) / std::min(s1.size(), s2.size());
}
