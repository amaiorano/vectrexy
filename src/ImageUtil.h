#pragma once

#include <memory>
#include <optional>

namespace ImageUtil {
    struct PngImageData {
        int width, height;
        bool hasAlpha;
        std::unique_ptr<uint8_t[]> data;
    };
    std::optional<PngImageData> loadPngImage(const char* name);

} // namespace ImageUtil
