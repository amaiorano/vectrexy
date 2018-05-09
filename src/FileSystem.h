#pragma once

#include <filesystem>

#if defined(_MSC_VER)
#if _MSC_VER >= 1914 // Visual Studio 2017 version 15.7
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

#else

namespace fs = std::filesystem;

#endif
