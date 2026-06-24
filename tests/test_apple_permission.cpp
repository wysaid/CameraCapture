/**
 * @file test_apple_permission.cpp
 * @brief Regression test for the macOS camera-permission request deadlock.
 *
 * ccap::runBlockingAsyncRequest() (used by ProviderApple::open) must run the
 * permission request on the calling thread and must NOT bounce it onto the main
 * dispatch queue. Otherwise Provider::open() hangs forever when called from a worker
 * thread in a process whose main thread is not running a run loop -- exactly the
 * situation a Node.js / Electron addon or any head-less multi-threaded embedder
 * creates.
 *
 * We exercise the real helper with a *simulated* asynchronous request: a short
 * countdown that fires the completion from a background thread, just like
 * AVCaptureDevice requestAccessForMediaType: delivers its completion off the caller's
 * run loop. No camera is required, so this runs deterministically in CI.
 *
 * On non-Apple platforms this file compiles to an empty translation unit.
 */

#if defined(__APPLE__)

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <thread>

#include "ccap_apple_async.h"

namespace
{

// Stand-in for AVCaptureDevice requestAccessForMediaType:completionHandler:: it fires
// the completion asynchronously from a *background* thread after a short countdown,
// never touching the caller's main run loop.
void simulateAsyncPermissionRequest(const std::function<void()>& done)
{
    std::function<void()> completion = done; // must outlive this call
    std::thread([completion]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // countdown
        completion();
    }).detach();
}

// Runs runBlockingAsyncRequest (optionally on a worker thread) and reports whether it
// returned within the timeout. A timeout means it deadlocked.
bool completesWithoutDeadlock(bool onWorkerThread, std::chrono::milliseconds timeout)
{
    std::promise<void> donePromise;
    std::future<void> doneFuture = donePromise.get_future();

    auto body = [&donePromise]() {
        ccap::runBlockingAsyncRequest(&simulateAsyncPermissionRequest);
        donePromise.set_value();
    };

    std::thread worker;
    if (onWorkerThread) {
        worker = std::thread(body);
    } else {
        body();
    }

    const bool completed = doneFuture.wait_for(timeout) == std::future_status::ready;
    if (worker.joinable()) {
        if (completed) {
            worker.join();
        } else {
            worker.detach(); // leave the hung thread; the process exits regardless
        }
    }
    return completed;
}

} // namespace

// The regression: open() called off the main thread with no run loop servicing the
// main queue. This deadlocked with the old dispatch-to-main-queue implementation.
TEST(AppleCameraPermission, OffMainThreadWithoutRunLoopDoesNotDeadlock)
{
    EXPECT_TRUE(completesWithoutDeadlock(/*onWorkerThread=*/true, std::chrono::seconds(5)))
        << "runBlockingAsyncRequest() deadlocked off the main thread -- the request was "
           "likely bounced onto an unserviced main dispatch queue.";
}

// Sanity: the common main-thread path must also complete promptly.
TEST(AppleCameraPermission, MainThreadDoesNotDeadlock)
{
    EXPECT_TRUE(completesWithoutDeadlock(/*onWorkerThread=*/false, std::chrono::seconds(5)))
        << "runBlockingAsyncRequest() deadlocked on the main thread.";
}

#endif // __APPLE__
