#include "args_parser.h"

#include <gtest/gtest.h>

TEST(CLIArgsParserTest, PreviewWithoutExplicitSizeRequests1080p) {
    char arg0[] = "ccap";
    char arg1[] = "--preview";
    char* argv[] = { arg0, arg1, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(2, argv);

    EXPECT_TRUE(opts.enablePreview);
    EXPECT_EQ(opts.width, 1920);
    EXPECT_EQ(opts.height, 1080);
    EXPECT_FALSE(opts.widthSpecified);
    EXPECT_FALSE(opts.heightSpecified);
}

TEST(CLIArgsParserTest, PreviewKeepsExplicitResolution) {
    char arg0[] = "ccap";
    char arg1[] = "--preview";
    char arg2[] = "--width";
    char arg3[] = "640";
    char arg4[] = "--height";
    char arg5[] = "480";
    char* argv[] = { arg0, arg1, arg2, arg3, arg4, arg5, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(6, argv);

    EXPECT_TRUE(opts.enablePreview);
    EXPECT_EQ(opts.width, 640);
    EXPECT_EQ(opts.height, 480);
    EXPECT_TRUE(opts.widthSpecified);
    EXPECT_TRUE(opts.heightSpecified);
}

TEST(CLIArgsParserTest, CaptureDefaultsStayUnchangedWithoutPreview) {
    char arg0[] = "ccap";
    char* argv[] = { arg0, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(1, argv);

    EXPECT_FALSE(opts.enablePreview);
    EXPECT_EQ(opts.width, 1280);
    EXPECT_EQ(opts.height, 720);
}

TEST(CLIArgsParserTest, ParsesJsonOutputOptions) {
    char arg0[] = "ccap";
    char arg1[] = "--json";
    char arg2[] = "--schema-version";
    char arg3[] = "1.2";
    char* argv[] = { arg0, arg1, arg2, arg3, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(4, argv);

    EXPECT_TRUE(opts.jsonOutput);
    EXPECT_EQ(opts.schemaVersion, "1.2");
}

#if defined(_WIN32) || defined(_WIN64)
TEST(CLIArgsParserTest, ParsesWindowsCameraBackendOption) {
    char arg0[] = "ccap";
    char arg1[] = "--camera-backend";
    char arg2[] = "msmf";
    char* argv[] = { arg0, arg1, arg2, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(3, argv);

    EXPECT_EQ(opts.windowsCameraBackend, "msmf");
}

TEST(CLIArgsParserTest, ParsesWindowsCameraBackendAlias) {
    char arg0[] = "ccap";
    char arg1[] = "--dshow";
    char* argv[] = { arg0, arg1, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(2, argv);

    EXPECT_EQ(opts.windowsCameraBackend, "dshow");
}

TEST(CLIArgsParserTest, LastWindowsCameraBackendOptionWins) {
    char arg0[] = "ccap";
    char arg1[] = "--msmf";
    char arg2[] = "--camera-backend";
    char arg3[] = "auto";
    char* argv[] = { arg0, arg1, arg2, arg3, nullptr };

    const ccap_cli::CLIOptions opts = ccap_cli::parseArgs(4, argv);

    EXPECT_EQ(opts.windowsCameraBackend, "auto");
}
#endif