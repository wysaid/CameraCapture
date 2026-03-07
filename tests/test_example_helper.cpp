#include "helper.h"

#include <gtest/gtest.h>

TEST(ExampleHelperTest, FiltersBackendFlagsFromArgumentList) {
    char arg0[] = "demo";
    char arg1[] = "--msmf";
    char arg2[] = "2";
    char arg3[] = "capture";
    char* argv[] = { arg0, arg1, arg2, arg3, nullptr };

    ExampleCommandLine commandLine{};
    initExampleCommandLine(&commandLine, 4, argv);

    EXPECT_EQ(commandLine.argc, 3);
    EXPECT_STREQ(commandLine.argv[0], "demo");
    EXPECT_STREQ(commandLine.argv[1], "2");
    EXPECT_STREQ(commandLine.argv[2], "capture");
#if defined(_WIN32) || defined(_WIN64)
    EXPECT_EQ(commandLine.cameraBackend, EXAMPLE_CAMERA_BACKEND_MSMF);
#else
    EXPECT_EQ(commandLine.cameraBackend, EXAMPLE_CAMERA_BACKEND_UNSPECIFIED);
#endif
}

TEST(ExampleHelperTest, LastBackendFlagWins) {
    char arg0[] = "demo";
    char arg1[] = "--msmf";
    char arg2[] = "--dshow";
    char arg3[] = "1";
    char* argv[] = { arg0, arg1, arg2, arg3, nullptr };

    ExampleCommandLine commandLine{};
    initExampleCommandLine(&commandLine, 4, argv);

    EXPECT_EQ(commandLine.argc, 2);
    EXPECT_STREQ(commandLine.argv[1], "1");
#if defined(_WIN32) || defined(_WIN64)
    EXPECT_EQ(commandLine.cameraBackend, EXAMPLE_CAMERA_BACKEND_DSHOW);
#else
    EXPECT_EQ(commandLine.cameraBackend, EXAMPLE_CAMERA_BACKEND_UNSPECIFIED);
#endif
}

TEST(ExampleHelperTest, ParsesDeviceIndexAfterFiltering) {
    char arg0[] = "demo";
    char arg1[] = "--auto";
    char arg2[] = "7";
    char* argv[] = { arg0, arg1, arg2, nullptr };

    ExampleCommandLine commandLine{};
    initExampleCommandLine(&commandLine, 3, argv);

    EXPECT_EQ(getExampleCameraIndex(&commandLine), 7);
}

TEST(ExampleHelperTest, RejectsNonNumericDeviceIndex) {
    char arg0[] = "demo";
    char arg1[] = "camera0";
    char* argv[] = { arg0, arg1, nullptr };

    ExampleCommandLine commandLine{};
    initExampleCommandLine(&commandLine, 2, argv);

    EXPECT_EQ(getExampleCameraIndex(&commandLine), -1);
}
