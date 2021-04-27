#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

// TsEvent provides a basic wait and signal cross-thread synchronization primitive.
class TsEvent {
public:
    // wait() resets the event, and blocks until the event is fired.
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        // fired = false;
        cv.wait(lock, [&] { return fired; });
    }

    // fire() signals the event, and unblocks any calls to wait().
    void fire() {
        std::unique_lock<std::mutex> lock(mutex);
        fired = true;
        cv.notify_all();
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    bool fired = false;
};
