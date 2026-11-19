/** Regression coverage for the real Apple camera authorization path. */
#include "ccap.h"
#include <gtest/gtest.h>

#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {
std::atomic<bool> requested{false};
std::atomic<int> requestCount{0};

AVAuthorizationStatus mockAuthorizationStatus(id, SEL, AVMediaType) {
    return requested.load() ? AVAuthorizationStatusDenied : AVAuthorizationStatusNotDetermined;
}

void mockRequestAccess(id, SEL, AVMediaType, void (^completion)(BOOL)) {
    ++requestCount;
    requested = true;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        completion(NO);
    });
}
} // namespace

TEST(AppleCameraPermission, WorkerWithoutMainRunLoopCanRetryAfterDenial) {
    // Re-exec to avoid running Objective-C in a child of a multithreaded process.
    // Isolate method replacements and bound the lifetime of a deadlocked child.
    const auto previousStyle = ::testing::GTEST_FLAG(death_test_style);
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    EXPECT_EXIT({
        @autoreleasepool {
            Method status = class_getClassMethod([AVCaptureDevice class], @selector(authorizationStatusForMediaType:));
            Method request = class_getClassMethod([AVCaptureDevice class], @selector(requestAccessForMediaType:completionHandler:));
            if (!status || !request) std::_Exit(2);
            method_setImplementation(status, reinterpret_cast<IMP>(mockAuthorizationStatus));
            method_setImplementation(request, reinterpret_cast<IMP>(mockRequestAccess));

            std::thread([] {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::_Exit(3);
            }).detach();
            bool first = true;
            bool second = true;
            std::thread worker([&] {
                ccap::Provider provider;
                first = provider.open("", false);
                requested = false;
                second = provider.open("", false);
            });
            worker.join();
            std::_Exit(!first && !second && requestCount == 2 ? 0 : 1);
        }
    }, ::testing::ExitedWithCode(0), "");
    ::testing::GTEST_FLAG(death_test_style) = previousStyle;
}
