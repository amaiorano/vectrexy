#include "core/ImageUtil.h"
#include "core/Base.h"
#include <cstdlib>
#include <cstring>

MSC_PUSH_WARNING_DISABLE(4100  // unreferenced formal parameter
                         4505) // unreferenced local function has been removed
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
MSC_POP_WARNING_DISABLE()

namespace ImageUtil {

    std::optional<PngImageData> loadPngImage(const char* name) {
        stbi_set_flip_vertically_on_load(1);
        int width{}, height{}, numChannels{};
        auto buffer = stbi_load(name, &width, &height, &numChannels, 0);

        size_t size = width * height * numChannels;
        auto data = std::make_unique<unsigned char[]>(size);
        memcpy(data.get(), buffer, size);

        stbi_image_free(buffer);

        return PngImageData{width, height, numChannels == 4, std::move(data)};
    }

} // namespace ImageUtil
