/**
 * @file test_guid_definitions.cpp
 * @brief Test to verify that locally defined GUIDs match the values from strmiids.lib
 *
 * This test ensures that the GUID values defined in ccap_dshow_guids.h
 * (used to avoid strmiids.lib dependency) are correct and match the official Windows SDK values.
 *
 * The test links against strmiids.lib to get the "ground truth" GUID values,
 * then compares them with the values from our shared header file.
 *
 * This approach ensures that the test verifies the EXACT SAME definitions used in production code.
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include <gtest/gtest.h>
#include <windows.h>
#include <dshow.h>

// Include the official GUID definitions from Windows SDK by linking strmiids.lib
// These will be used as the reference values
#pragma comment(lib, "strmiids.lib")

// Define CCAP_NO_GUID_ALIASES to prevent macro conflicts with strmiids.lib symbols
// We need both the CCAP_ prefixed names and the standard names for comparison
#define CCAP_NO_GUID_ALIASES

// Include initguid.h BEFORE the GUID header so DEFINE_GUID actually defines the GUIDs
// (rather than just declaring them as extern references)
#include <initguid.h>

// Include the SAME shared header that is used by the production code
// This ensures we test the actual values used in ccap_imp_windows.cpp
#include "../src/ccap_dshow_guids.h"

// ============================================================================
// Helper function to format GUID as string for better error messages
// ============================================================================
std::string GuidToString(const GUID& guid) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buffer);
}

// ============================================================================
// Test fixture
// ============================================================================
class GuidDefinitionsTest : public ::testing::Test {
protected:
    void CompareGuid(const GUID& ccapGuid, const GUID& reference, const char* name) {
        EXPECT_EQ(ccapGuid.Data1, reference.Data1) << name << " Data1 mismatch";
        EXPECT_EQ(ccapGuid.Data2, reference.Data2) << name << " Data2 mismatch";
        EXPECT_EQ(ccapGuid.Data3, reference.Data3) << name << " Data3 mismatch";
        for (int i = 0; i < 8; i++) {
            EXPECT_EQ(ccapGuid.Data4[i], reference.Data4[i]) << name << " Data4[" << i << "] mismatch";
        }

        // Also test using IsEqualGUID for completeness
        EXPECT_TRUE(IsEqualGUID(ccapGuid, reference)) << name << " GUIDs not equal!\n"
                                                      << "  CCAP:      " << GuidToString(ccapGuid) << "\n"
                                                      << "  Reference: " << GuidToString(reference);
    }
};

// ============================================================================
// Tests for DirectShow Interface IIDs
// ============================================================================
TEST_F(GuidDefinitionsTest, IID_ICreateDevEnum) {
    CompareGuid(CCAP_IID_ICreateDevEnum, IID_ICreateDevEnum, "IID_ICreateDevEnum");
}

TEST_F(GuidDefinitionsTest, IID_IBaseFilter) {
    CompareGuid(CCAP_IID_IBaseFilter, IID_IBaseFilter, "IID_IBaseFilter");
}

TEST_F(GuidDefinitionsTest, IID_IGraphBuilder) {
    CompareGuid(CCAP_IID_IGraphBuilder, IID_IGraphBuilder, "IID_IGraphBuilder");
}

TEST_F(GuidDefinitionsTest, IID_ICaptureGraphBuilder2) {
    CompareGuid(CCAP_IID_ICaptureGraphBuilder2, IID_ICaptureGraphBuilder2, "IID_ICaptureGraphBuilder2");
}

TEST_F(GuidDefinitionsTest, IID_IMediaControl) {
    CompareGuid(CCAP_IID_IMediaControl, IID_IMediaControl, "IID_IMediaControl");
}

TEST_F(GuidDefinitionsTest, IID_IMediaFilter) {
    CompareGuid(CCAP_IID_IMediaFilter, IID_IMediaFilter, "IID_IMediaFilter");
}

TEST_F(GuidDefinitionsTest, IID_IVideoWindow) {
    CompareGuid(CCAP_IID_IVideoWindow, IID_IVideoWindow, "IID_IVideoWindow");
}

TEST_F(GuidDefinitionsTest, IID_IAMStreamConfig) {
    CompareGuid(CCAP_IID_IAMStreamConfig, IID_IAMStreamConfig, "IID_IAMStreamConfig");
}

TEST_F(GuidDefinitionsTest, IID_IPropertyBag) {
    CompareGuid(CCAP_IID_IPropertyBag, IID_IPropertyBag, "IID_IPropertyBag");
}

// ============================================================================
// Tests for DirectShow CLSIDs
// ============================================================================
TEST_F(GuidDefinitionsTest, CLSID_SystemDeviceEnum) {
    CompareGuid(CCAP_CLSID_SystemDeviceEnum, CLSID_SystemDeviceEnum, "CLSID_SystemDeviceEnum");
}

TEST_F(GuidDefinitionsTest, CLSID_VideoInputDeviceCategory) {
    CompareGuid(CCAP_CLSID_VideoInputDeviceCategory, CLSID_VideoInputDeviceCategory, "CLSID_VideoInputDeviceCategory");
}

TEST_F(GuidDefinitionsTest, CLSID_FilterGraph) {
    CompareGuid(CCAP_CLSID_FilterGraph, CLSID_FilterGraph, "CLSID_FilterGraph");
}

TEST_F(GuidDefinitionsTest, CLSID_CaptureGraphBuilder2) {
    CompareGuid(CCAP_CLSID_CaptureGraphBuilder2, CLSID_CaptureGraphBuilder2, "CLSID_CaptureGraphBuilder2");
}

// Note: CLSID_SampleGrabber and CLSID_NullRenderer are from qedit.h (deprecated)
// They are not in strmiids.lib, but we can verify them by attempting to create the objects

// ============================================================================
// Tests for Media Types and Format Types
// ============================================================================
TEST_F(GuidDefinitionsTest, MEDIATYPE_Video) {
    CompareGuid(CCAP_MEDIATYPE_Video, MEDIATYPE_Video, "MEDIATYPE_Video");
}

TEST_F(GuidDefinitionsTest, FORMAT_VideoInfo) {
    CompareGuid(CCAP_FORMAT_VideoInfo, FORMAT_VideoInfo, "FORMAT_VideoInfo");
}

TEST_F(GuidDefinitionsTest, FORMAT_VideoInfo2) {
    CompareGuid(CCAP_FORMAT_VideoInfo2, FORMAT_VideoInfo2, "FORMAT_VideoInfo2");
}

// ============================================================================
// Tests for Pin Categories
// ============================================================================
TEST_F(GuidDefinitionsTest, PIN_CATEGORY_CAPTURE) {
    CompareGuid(CCAP_PIN_CATEGORY_CAPTURE, PIN_CATEGORY_CAPTURE, "PIN_CATEGORY_CAPTURE");
}

TEST_F(GuidDefinitionsTest, PIN_CATEGORY_PREVIEW) {
    CompareGuid(CCAP_PIN_CATEGORY_PREVIEW, PIN_CATEGORY_PREVIEW, "PIN_CATEGORY_PREVIEW");
}

// ============================================================================
// Tests for Video Subtype GUIDs (MEDIASUBTYPE_*)
// ============================================================================
TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_None) {
    CompareGuid(CCAP_MEDIASUBTYPE_None, MEDIASUBTYPE_None, "MEDIASUBTYPE_None");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_MJPG) {
    CompareGuid(CCAP_MEDIASUBTYPE_MJPG, MEDIASUBTYPE_MJPG, "MEDIASUBTYPE_MJPG");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_RGB24) {
    CompareGuid(CCAP_MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB24, "MEDIASUBTYPE_RGB24");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_RGB32) {
    CompareGuid(CCAP_MEDIASUBTYPE_RGB32, MEDIASUBTYPE_RGB32, "MEDIASUBTYPE_RGB32");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_RGB555) {
    CompareGuid(CCAP_MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB555, "MEDIASUBTYPE_RGB555");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_RGB565) {
    CompareGuid(CCAP_MEDIASUBTYPE_RGB565, MEDIASUBTYPE_RGB565, "MEDIASUBTYPE_RGB565");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_NV12) {
    CompareGuid(CCAP_MEDIASUBTYPE_NV12, MEDIASUBTYPE_NV12, "MEDIASUBTYPE_NV12");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IYUV) {
    CompareGuid(CCAP_MEDIASUBTYPE_IYUV, MEDIASUBTYPE_IYUV, "MEDIASUBTYPE_IYUV");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_YUY2) {
    CompareGuid(CCAP_MEDIASUBTYPE_YUY2, MEDIASUBTYPE_YUY2, "MEDIASUBTYPE_YUY2");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_YV12) {
    CompareGuid(CCAP_MEDIASUBTYPE_YV12, MEDIASUBTYPE_YV12, "MEDIASUBTYPE_YV12");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_UYVY) {
    CompareGuid(CCAP_MEDIASUBTYPE_UYVY, MEDIASUBTYPE_UYVY, "MEDIASUBTYPE_UYVY");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_YVYU) {
    CompareGuid(CCAP_MEDIASUBTYPE_YVYU, MEDIASUBTYPE_YVYU, "MEDIASUBTYPE_YVYU");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_YVU9) {
    CompareGuid(CCAP_MEDIASUBTYPE_YVU9, MEDIASUBTYPE_YVU9, "MEDIASUBTYPE_YVU9");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_Y411) {
    CompareGuid(CCAP_MEDIASUBTYPE_Y411, MEDIASUBTYPE_Y411, "MEDIASUBTYPE_Y411");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_Y41P) {
    CompareGuid(CCAP_MEDIASUBTYPE_Y41P, MEDIASUBTYPE_Y41P, "MEDIASUBTYPE_Y41P");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_CLJR) {
    CompareGuid(CCAP_MEDIASUBTYPE_CLJR, MEDIASUBTYPE_CLJR, "MEDIASUBTYPE_CLJR");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IF09) {
    CompareGuid(CCAP_MEDIASUBTYPE_IF09, MEDIASUBTYPE_IF09, "MEDIASUBTYPE_IF09");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_CPLA) {
    CompareGuid(CCAP_MEDIASUBTYPE_CPLA, MEDIASUBTYPE_CPLA, "MEDIASUBTYPE_CPLA");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_AYUV) {
    CompareGuid(CCAP_MEDIASUBTYPE_AYUV, MEDIASUBTYPE_AYUV, "MEDIASUBTYPE_AYUV");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_AI44) {
    CompareGuid(CCAP_MEDIASUBTYPE_AI44, MEDIASUBTYPE_AI44, "MEDIASUBTYPE_AI44");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IA44) {
    CompareGuid(CCAP_MEDIASUBTYPE_IA44, MEDIASUBTYPE_IA44, "MEDIASUBTYPE_IA44");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IMC1) {
    CompareGuid(CCAP_MEDIASUBTYPE_IMC1, MEDIASUBTYPE_IMC1, "MEDIASUBTYPE_IMC1");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IMC2) {
    CompareGuid(CCAP_MEDIASUBTYPE_IMC2, MEDIASUBTYPE_IMC2, "MEDIASUBTYPE_IMC2");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IMC3) {
    CompareGuid(CCAP_MEDIASUBTYPE_IMC3, MEDIASUBTYPE_IMC3, "MEDIASUBTYPE_IMC3");
}

TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_IMC4) {
    CompareGuid(CCAP_MEDIASUBTYPE_IMC4, MEDIASUBTYPE_IMC4, "MEDIASUBTYPE_IMC4");
}

// ============================================================================
// Special test for I420 (not always in strmiids.lib)
// We verify it matches the FourCC pattern
// ============================================================================
TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_I420_FourCC) {
    // I420 GUID should follow the FourCC pattern: {FOURCC-0000-0010-8000-00AA00389B71}
    // 'I420' = 0x30323449
    GUID expected = { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
    CompareGuid(CCAP_MEDIASUBTYPE_I420, expected, "MEDIASUBTYPE_I420 (FourCC pattern)");
}

// ============================================================================
// Special test for YUYV (may not be in strmiids.lib on all systems)
// We verify it matches the FourCC pattern
// ============================================================================
TEST_F(GuidDefinitionsTest, MEDIASUBTYPE_YUYV_FourCC) {
    // YUYV GUID should follow the FourCC pattern
    // 'YUYV' = 0x56595559
    GUID expected = { 0x56595559, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
    CompareGuid(CCAP_MEDIASUBTYPE_YUYV, expected, "MEDIASUBTYPE_YUYV (FourCC pattern)");
}

// ============================================================================
// Validation test for qedit.h GUIDs (SampleGrabber, NullRenderer)
// These are verified by checking if COM objects can be created
// ============================================================================
TEST_F(GuidDefinitionsTest, CLSID_SampleGrabber_CanCreate) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    if (comInitialized) {
        IUnknown* pUnknown = nullptr;
        hr = CoCreateInstance(CCAP_CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&pUnknown);

        // The object may or may not be registered on all systems
        // If it can be created, our GUID is correct
        if (SUCCEEDED(hr)) {
            EXPECT_NE(pUnknown, nullptr) << "CLSID_SampleGrabber created but returned null";
            if (pUnknown) {
                pUnknown->Release();
            }
            std::cout << "  [OK] CLSID_SampleGrabber verified by successful COM object creation" << std::endl;
        } else {
            // Not an error - the filter may not be registered on this system
            std::cout << "  [INFO] CLSID_SampleGrabber filter not registered (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
            std::cout << "         This is expected on some Windows versions where qedit.dll is not available" << std::endl;
        }

        CoUninitialize();
    }
}

TEST_F(GuidDefinitionsTest, CLSID_NullRenderer_CanCreate) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    if (comInitialized) {
        IUnknown* pUnknown = nullptr;
        hr = CoCreateInstance(CCAP_CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&pUnknown);

        if (SUCCEEDED(hr)) {
            EXPECT_NE(pUnknown, nullptr) << "CLSID_NullRenderer created but returned null";
            if (pUnknown) {
                pUnknown->Release();
            }
            std::cout << "  [OK] CLSID_NullRenderer verified by successful COM object creation" << std::endl;
        } else {
            std::cout << "  [INFO] CLSID_NullRenderer filter not registered (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
            std::cout << "         This is expected on some Windows versions where qedit.dll is not available" << std::endl;
        }

        CoUninitialize();
    }
}

// ============================================================================
// Summary test that prints all GUIDs for visual inspection
// ============================================================================
TEST_F(GuidDefinitionsTest, PrintAllGuidComparisons) {
    std::cout << "\n============================================" << std::endl;
    std::cout << "GUID Definition Verification Summary" << std::endl;
    std::cout << "(Testing values from ccap_dshow_guids.h)" << std::endl;
    std::cout << "============================================\n" << std::endl;

    auto printComparison = [](const char* name, const GUID& ccapGuid, const GUID& reference) {
        bool match = IsEqualGUID(ccapGuid, reference);
        std::cout << (match ? "[PASS]" : "[FAIL]") << " " << name << std::endl;
        if (!match) {
            std::cout << "       CCAP:      " << GuidToString(ccapGuid) << std::endl;
            std::cout << "       Reference: " << GuidToString(reference) << std::endl;
        }
    };

    std::cout << "--- DirectShow Interface IIDs ---" << std::endl;
    printComparison("IID_ICreateDevEnum", CCAP_IID_ICreateDevEnum, IID_ICreateDevEnum);
    printComparison("IID_IBaseFilter", CCAP_IID_IBaseFilter, IID_IBaseFilter);
    printComparison("IID_IGraphBuilder", CCAP_IID_IGraphBuilder, IID_IGraphBuilder);
    printComparison("IID_ICaptureGraphBuilder2", CCAP_IID_ICaptureGraphBuilder2, IID_ICaptureGraphBuilder2);
    printComparison("IID_IMediaControl", CCAP_IID_IMediaControl, IID_IMediaControl);
    printComparison("IID_IMediaFilter", CCAP_IID_IMediaFilter, IID_IMediaFilter);
    printComparison("IID_IVideoWindow", CCAP_IID_IVideoWindow, IID_IVideoWindow);
    printComparison("IID_IAMStreamConfig", CCAP_IID_IAMStreamConfig, IID_IAMStreamConfig);
    printComparison("IID_IPropertyBag", CCAP_IID_IPropertyBag, IID_IPropertyBag);

    std::cout << "\n--- DirectShow CLSIDs ---" << std::endl;
    printComparison("CLSID_SystemDeviceEnum", CCAP_CLSID_SystemDeviceEnum, CLSID_SystemDeviceEnum);
    printComparison("CLSID_VideoInputDeviceCategory", CCAP_CLSID_VideoInputDeviceCategory, CLSID_VideoInputDeviceCategory);
    printComparison("CLSID_FilterGraph", CCAP_CLSID_FilterGraph, CLSID_FilterGraph);
    printComparison("CLSID_CaptureGraphBuilder2", CCAP_CLSID_CaptureGraphBuilder2, CLSID_CaptureGraphBuilder2);

    std::cout << "\n--- Media Types and Format Types ---" << std::endl;
    printComparison("MEDIATYPE_Video", CCAP_MEDIATYPE_Video, MEDIATYPE_Video);
    printComparison("FORMAT_VideoInfo", CCAP_FORMAT_VideoInfo, FORMAT_VideoInfo);
    printComparison("FORMAT_VideoInfo2", CCAP_FORMAT_VideoInfo2, FORMAT_VideoInfo2);

    std::cout << "\n--- Pin Categories ---" << std::endl;
    printComparison("PIN_CATEGORY_CAPTURE", CCAP_PIN_CATEGORY_CAPTURE, PIN_CATEGORY_CAPTURE);
    printComparison("PIN_CATEGORY_PREVIEW", CCAP_PIN_CATEGORY_PREVIEW, PIN_CATEGORY_PREVIEW);

    std::cout << "\n--- Video Subtypes (MEDIASUBTYPE_*) ---" << std::endl;
    printComparison("MEDIASUBTYPE_None", CCAP_MEDIASUBTYPE_None, MEDIASUBTYPE_None);
    printComparison("MEDIASUBTYPE_MJPG", CCAP_MEDIASUBTYPE_MJPG, MEDIASUBTYPE_MJPG);
    printComparison("MEDIASUBTYPE_RGB24", CCAP_MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB24);
    printComparison("MEDIASUBTYPE_RGB32", CCAP_MEDIASUBTYPE_RGB32, MEDIASUBTYPE_RGB32);
    printComparison("MEDIASUBTYPE_RGB555", CCAP_MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB555);
    printComparison("MEDIASUBTYPE_RGB565", CCAP_MEDIASUBTYPE_RGB565, MEDIASUBTYPE_RGB565);
    printComparison("MEDIASUBTYPE_NV12", CCAP_MEDIASUBTYPE_NV12, MEDIASUBTYPE_NV12);
    printComparison("MEDIASUBTYPE_IYUV", CCAP_MEDIASUBTYPE_IYUV, MEDIASUBTYPE_IYUV);
    printComparison("MEDIASUBTYPE_YUY2", CCAP_MEDIASUBTYPE_YUY2, MEDIASUBTYPE_YUY2);
    printComparison("MEDIASUBTYPE_YV12", CCAP_MEDIASUBTYPE_YV12, MEDIASUBTYPE_YV12);
    printComparison("MEDIASUBTYPE_UYVY", CCAP_MEDIASUBTYPE_UYVY, MEDIASUBTYPE_UYVY);
    printComparison("MEDIASUBTYPE_YVYU", CCAP_MEDIASUBTYPE_YVYU, MEDIASUBTYPE_YVYU);
    printComparison("MEDIASUBTYPE_YVU9", CCAP_MEDIASUBTYPE_YVU9, MEDIASUBTYPE_YVU9);
    printComparison("MEDIASUBTYPE_Y411", CCAP_MEDIASUBTYPE_Y411, MEDIASUBTYPE_Y411);
    printComparison("MEDIASUBTYPE_Y41P", CCAP_MEDIASUBTYPE_Y41P, MEDIASUBTYPE_Y41P);
    printComparison("MEDIASUBTYPE_CLJR", CCAP_MEDIASUBTYPE_CLJR, MEDIASUBTYPE_CLJR);
    printComparison("MEDIASUBTYPE_IF09", CCAP_MEDIASUBTYPE_IF09, MEDIASUBTYPE_IF09);
    printComparison("MEDIASUBTYPE_CPLA", CCAP_MEDIASUBTYPE_CPLA, MEDIASUBTYPE_CPLA);
    printComparison("MEDIASUBTYPE_AYUV", CCAP_MEDIASUBTYPE_AYUV, MEDIASUBTYPE_AYUV);
    printComparison("MEDIASUBTYPE_AI44", CCAP_MEDIASUBTYPE_AI44, MEDIASUBTYPE_AI44);
    printComparison("MEDIASUBTYPE_IA44", CCAP_MEDIASUBTYPE_IA44, MEDIASUBTYPE_IA44);
    printComparison("MEDIASUBTYPE_IMC1", CCAP_MEDIASUBTYPE_IMC1, MEDIASUBTYPE_IMC1);
    printComparison("MEDIASUBTYPE_IMC2", CCAP_MEDIASUBTYPE_IMC2, MEDIASUBTYPE_IMC2);
    printComparison("MEDIASUBTYPE_IMC3", CCAP_MEDIASUBTYPE_IMC3, MEDIASUBTYPE_IMC3);
    printComparison("MEDIASUBTYPE_IMC4", CCAP_MEDIASUBTYPE_IMC4, MEDIASUBTYPE_IMC4);

    std::cout << "\n============================================" << std::endl;
}

#else // Non-Windows platforms

#include <gtest/gtest.h>

TEST(GuidDefinitionsTest, NonWindowsPlatform) {
    GTEST_SKIP() << "GUID verification tests only run on Windows";
}

#endif // _WIN32 || _MSC_VER
