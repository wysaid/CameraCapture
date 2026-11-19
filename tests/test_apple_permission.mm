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
bool authorized = false;
int configurationDepth = 0;

AVAuthorizationStatus mockAuthorizationStatus(id, SEL, AVMediaType) {
    if (authorized) return AVAuthorizationStatusAuthorized;
    return requested.load() ? AVAuthorizationStatusDenied : AVAuthorizationStatusNotDetermined;
}

void mockRequestAccess(id, SEL, AVMediaType, void (^completion)(BOOL)) {
    ++requestCount;
    requested = true;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        completion(NO);
    });
}

AVCaptureDevice* mockDefaultDevice(id, SEL, AVMediaType) { return nil; }

void mockBeginConfiguration(id, SEL) { ++configurationDepth; }

void mockCommitConfiguration(id, SEL) { --configurationDepth; }
} // namespace

TEST(AppleCameraPermission, WorkerWithoutMainRunLoopCanRetryAfterFailedOpen) {
    // Re-exec to avoid running Objective-C in a child of a multithreaded process.
    // Isolate method replacements and bound the lifetime of a deadlocked child.
    const auto previousStyle = ::testing::GTEST_FLAG(death_test_style);
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    EXPECT_EXIT({
        @autoreleasepool {
            Method status = class_getClassMethod([AVCaptureDevice class], @selector(authorizationStatusForMediaType:));
            Method request = class_getClassMethod([AVCaptureDevice class], @selector(requestAccessForMediaType:completionHandler:));
            Method device = class_getClassMethod([AVCaptureDevice class], @selector(defaultDeviceWithMediaType:));
            Class sessionClass = object_getClass([[AVCaptureSession alloc] init]);
            Method begin = class_getInstanceMethod(sessionClass, @selector(beginConfiguration));
            Method commit = class_getInstanceMethod(sessionClass, @selector(commitConfiguration));
            if (!status || !request || !device || !begin || !commit) std::_Exit(2);
            method_setImplementation(status, reinterpret_cast<IMP>(mockAuthorizationStatus));
            method_setImplementation(request, reinterpret_cast<IMP>(mockRequestAccess));
            method_setImplementation(device, reinterpret_cast<IMP>(mockDefaultDevice));
            method_setImplementation(begin, reinterpret_cast<IMP>(mockBeginConfiguration));
            method_setImplementation(commit, reinterpret_cast<IMP>(mockCommitConfiguration));

            std::thread([] {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::_Exit(3);
            }).detach();
            std::thread worker([] {
                ccap::Provider provider;
                if (provider.open("", false)) std::_Exit(1);
                requested = false;
                if (provider.open("", false) || requestCount != 2) std::_Exit(1);

                authorized = true;
                if (provider.open("", false) || configurationDepth != 0) std::_Exit(4);
                if (provider.open("", false) || configurationDepth != 0) std::_Exit(4);
            });
            worker.join();
            std::_Exit(0);
        }
    }, ::testing::ExitedWithCode(0), "");
    ::testing::GTEST_FLAG(death_test_style) = previousStyle;
}
