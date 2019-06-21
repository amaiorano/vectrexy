#include "engine/Overlays.h"
#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/StringUtil.h"
#include <string_view>
#include <unordered_map>

namespace {

    // To avoid performing the computation multiple times on the same pair of substrings, we cache
    // the results and look them up before computing
    using LDCache = std::unordered_map<std::string_view, std::unordered_map<std::string_view, int>>;

    int LevenshteinDistance(std::string_view s, std::string_view t, LDCache& cache) {

        // base case: empty strings
        if (s.size() == 0)
            return static_cast<int>(t.size());

        if (t.size() == 0)
            return static_cast<int>(s.size());

        auto& cache1 = cache[s];
        if (auto iter = cache1.find(t); iter != cache1.end()) {
            return iter->second;
        }

        // test if last characters of the strings match
        int cost = s.back() == t.back() ? 0 : 1;

        auto min3 = [](auto a, auto b, auto c) { return std::min(std::min(a, b), std::min(b, c)); };

        // return minimum of delete char from s, delete char from t, and delete char from both
        auto result =
            min3(LevenshteinDistance(s.substr(0, s.size() - 1), t, cache) + 1,
                 LevenshteinDistance(s, t.substr(0, t.size() - 1), cache) + 1,
                 LevenshteinDistance(s.substr(0, s.size() - 1), t.substr(0, t.size() - 1), cache) +
                     cost);

        cache1[t] = result;
        return result;
    }

    int LevenshteinDistance(std::string_view s, std::string_view t) {
        LDCache cache;
        return LevenshteinDistance(s, t, cache);
    }

    // Returns a confidence ratio in [0.0, 1.0] from no match to perfect match
    float OverlayFileFuzzyMatch(const fs::path& p1, const fs::path& p2) {
        auto TrimFileName = [](std::string s) {
            // Replace separators with space
            s = StringUtil::Replace(s, "-", " ");
            s = StringUtil::Replace(s, "_", " ");

            std::string_view sub = s;

            // Remove " by GCE"
            auto index = s.find(" by GCE");
            if (index != std::string::npos) {
                sub = sub.substr(0, index);
            }
            // Remove extra version details at end of name, e.g. "(PD)"
            index = s.find(" (");
            if (index != std::string::npos) {
                sub = sub.substr(0, index);
            }

            // Remove extension, e.g. ".png"
            index = s.rfind('.');
            if (index != std::string::npos) {
                sub = sub.substr(0, index);
            }

            return std::string{sub};
        };

        using namespace StringUtil;
        auto t1 = Remove(ToLower(TrimFileName(p1.filename().string())), " ");
        auto t2 = Remove(ToLower(TrimFileName(p2.filename().string())), " ");
        auto dist = LevenshteinDistance(t1, t2);

        auto distRatio = 1.0f - static_cast<float>(dist) / std::max(t1.size(), t2.size());
        return distRatio;
    }
} // namespace

void Overlays::LoadOverlays(const fs::path& overlaysDir) {
    if (!fs::exists(overlaysDir))
        return;

    for (auto& dir : fs::recursive_directory_iterator(overlaysDir)) {
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

    // Errorf("Overlay bestConfidence: %f\n", bestConfidence);
    if (bestConfidence > 0.5f) {
        return bestMatch;
    }
    return {};
}
