#pragma once

#include <filesystem>

//@TODO: should be std::filesystem
namespace fs = std::experimental::filesystem;

namespace FileSystemUtil {
    // Saves current directory, changes to directory of input path (if non-empty),
    // then restores original directory on destruction.
    struct ScopedSetCurrentDirectory {
        ScopedSetCurrentDirectory(fs::path newPath) {
            m_lastDir = fs::current_path();
            if (!newPath.empty())
                fs::current_path(newPath.remove_filename());
        }

        ~ScopedSetCurrentDirectory() { fs::current_path(m_lastDir); }

    private:
        fs::path m_lastDir;
    };
} // namespace FileSystemUtil
