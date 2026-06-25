/**
 * @file test_apple_permission.cpp
 * @brief Contract test for ccap::runBlockingAsyncRequest() -- the helper that
 *        ProviderApple::open() delegates its camera-permission wait to on macOS.
 *
 * Background: open()'s permission request used to be dispatched onto the main dispatch
 * queue, which deadlocks when nothing services that queue (a worker thread in a
 * Node.js / Electron addon, or a head-less service). The fix extracted the
 * "start an async request and block until it completes" step into
 * runBlockingAsyncRequest(), which runs the request on the calling thread.
 *
 * Scope: this pins that helper's contract -- a blocking wait whose completion is
 * delivered on another thread must finish without deadlocking or missing the signal --
 * using a *simulated* async request (a background-thread countdown standing in for
 * AVCaptureDevice requestAccessForMediaType:). It deliberately does NOT drive
 * open()/AVFoundation end to end: that needs a real camera and TCC state and cannot run
 * deterministically in CI. The helper is the unit where the deadlock lived once the
 * main-queue hop was removed, so guarding its contract is what is testable here.
 *
 * On non-Apple platforms this file compiles to an empty translation unit.
 */

#if defined(__APPLE__)

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "ccap_apple_async.h"

namespace
{

// Runs `scenario` (which performs the runBlockingAsyncRequest call under test) on a
// dedicated worker thread and reports whether it finished within `timeout`. Keeping the
// call on a worker thread -- with the watchdog on the calling thread -- means a
// regression that deadlocks fails the test with a clean timeout instead of hanging the
// whole test binary. The completion state lives on the heap and is shared with the
// worker, so a late completion after a timeout/detach can never touch freed state.
bool finishesWithinTimeout(std::function<void()> scenario, std::chrono::milliseconds timeout)
{
    auto finished = std::make_shared<std::promise<void>>();
    std::future<void> future = finished->get_future();

    std::thread worker([scenario = std::move(scenario), finished]() {
        scenario();
        finished->set_value();
    });

    const bool ok = future.wait_for(timeout) == std::future_status::ready;
    if (ok) {
        worker.join();
    } else {
        worker.detach(); // never block the test process; the heap state keeps detach safe
    }
    return ok;
}

// Stand-in for AVCaptureDevice requestAccessForMediaType:completionHandler:: fires the
// completion asynchronously from a *background* thread after a short countdown, exactly
// like the real API delivers its completion off the caller's run loop.
void completeAsynchronously(const std::function<void()>& done)
{
    auto completion = std::make_shared<std::function<void()>>(done);
    std::thread([completion]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // countdown
        (*completion)();
    }).detach();
}

} // namespace

// Regression: the permission wait must not deadlock when run off the main thread with
// no run loop servicing the main queue (e.g. a ccap::Provider opened from a Node.js
// addon worker thread). This hung with the old dispatch-to-main-queue implementation.
TEST(AppleCameraPermission, AsyncCompletionOffMainThreadDoesNotDeadlock)
{
    EXPECT_TRUE(finishesWithinTimeout([] { ccap::runBlockingAsyncRequest(&completeAsynchronously); },
                                      std::chrono::seconds(5)))
        << "runBlockingAsyncRequest() deadlocked -- the request was likely bounced onto "
           "an unserviced main dispatch queue.";
}

// The completion may also fire synchronously (e.g. authorization already determined);
// the blocking wait must still observe the signal rather than miss it.
TEST(AppleCameraPermission, SynchronousCompletionDoesNotDeadlock)
{
    EXPECT_TRUE(finishesWithinTimeout(
                    [] { ccap::runBlockingAsyncRequest([](const std::function<void()>& done) { done(); }); },
                    std::chrono::seconds(5)))
        << "runBlockingAsyncRequest() missed a synchronous completion.";
}

#endif // __APPLE__
