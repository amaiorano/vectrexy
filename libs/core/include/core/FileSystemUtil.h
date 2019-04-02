#pragma once

#include "core/FileSystem.h"

namespace FileSystemUtil {
    // Saves current directory, changes to directory of input path (if non-empty),
    // then restores original directory on destruction. Input path may be a path to a directory or
    // to a file.
    struct ScopedSetCurrentDirectory {
        ScopedSetCurrentDirectory(fs::path newPath = {}) {
            m_lastDir = fs::current_path();
            if (!newPath.empty()) {
                if (fs::is_regular_file(newPath))
                    newPath = newPath.remove_filename();

                fs::current_path(newPath);
            }
        }

        ~ScopedSetCurrentDirectory() { fs::current_path(m_lastDir); }

    private:
        fs::path m_lastDir;
    };
} // namespace FileSystemUtil
