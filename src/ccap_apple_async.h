/**
 * @file ccap_apple_async.h
 * @author wysaid (this@wysaid.org)
 * @brief Run a callback-based asynchronous request and block until it completes,
 *        without bouncing the request onto the main dispatch queue.
 *
 * macOS callback APIs such as `AVCaptureDevice requestAccessForMediaType:` deliver
 * their completion on an internal queue, not on the caller's run loop. ccap used to
 * dispatch the permission request onto the main queue for non-main-thread callers,
 * which deadlocks whenever nothing is servicing that queue -- e.g. a ccap::Provider
 * opened from a worker thread in a process that has no CFRunLoop on its main thread
 * (a Node.js / Electron addon, a head-less multi-threaded service, ...).
 *
 * runBlockingAsyncRequest() starts the request on the *calling* thread and blocks on a
 * portable condition variable until the supplied continuation is invoked, so it is
 * safe to call from any thread regardless of run-loop state.
 *
 * Covered by tests/test_apple_permission.cpp.
 */

#pragma once

#if defined(__APPLE__)

#include <condition_variable>
#include <functional>
#include <mutex>

namespace ccap
{

/**
 * Invoke @p start on the current thread and block until the continuation that
 * @p start receives (its `done` argument) is called. @p start may invoke `done` from
 * any thread or queue. The request is never dispatched to the main queue, so this
 * cannot deadlock when no run loop is servicing it.
 */
inline void runBlockingAsyncRequest(const std::function<void(const std::function<void()>& done)>& start)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;

    start([&mutex, &cv, &finished]() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            finished = true;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&finished]() { return finished; });
}

} // namespace ccap

#endif // __APPLE__
