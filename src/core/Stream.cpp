#include "core/Stream.h"

void IStream::Printf(const char* format, ...) {
    static char buffer[2048];
    va_list args;
    va_start(args, format);
    int bytesWritten = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    assert(bytesWritten < sizeof(buffer));

    WriteImpl(buffer, bytesWritten, 1);
}
