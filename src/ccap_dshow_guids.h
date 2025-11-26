/**
 * @file ccap_dshow_guids.h
 * @author wysaid (this@wysaid.org)
 * @brief DirectShow GUID definitions to avoid strmiids.lib dependency.
 * @date 2025-04
 *
 * This header defines all DirectShow GUIDs required by ccap locally,
 * eliminating the need to link against strmiids.lib. This is particularly
 * useful for MinGW/cross-compilation scenarios where the library may not
 * be available or causes linking issues with static libraries.
 *
 * GUID values are sourced from Windows SDK headers (uuids.h, strmif.h, etc.)
 * and are verified by unit tests against strmiids.lib on MSVC.
 *
 * Usage:
 *   - Include <initguid.h> BEFORE including this header in exactly ONE .cpp file
 *   - This ensures GUIDs are defined (not just declared) in that compilation unit
 */

#pragma once

#ifndef CCAP_DSHOW_GUIDS_H
#define CCAP_DSHOW_GUIDS_H

#if defined(_WIN32) || defined(_MSC_VER)

#include <guiddef.h>

// clang-format off

// ============================================================================
// DirectShow Interface IIDs
// ============================================================================
DEFINE_GUID(CCAP_IID_ICreateDevEnum,       0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CCAP_IID_IBaseFilter,          0x56a86895, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_IID_IGraphBuilder,        0x56a868a9, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_IID_ICaptureGraphBuilder2, 0x93e5a4e0, 0x2d50, 0x11d2, 0xab, 0xfa, 0x00, 0xa0, 0xc9, 0xc6, 0xe3, 0x8d);
DEFINE_GUID(CCAP_IID_IMediaControl,        0x56a868b1, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_IID_IMediaFilter,         0x56a86899, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_IID_IVideoWindow,         0x56a868b4, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_IID_IAMStreamConfig,      0xc6e13340, 0x30ac, 0x11d0, 0xa1, 0x8c, 0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56);
DEFINE_GUID(CCAP_IID_IPropertyBag,         0x55272a00, 0x42cb, 0x11ce, 0x81, 0x35, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51);
DEFINE_GUID(CCAP_IID_ISampleGrabber,       0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);

// ============================================================================
// DirectShow CLSIDs
// ============================================================================
DEFINE_GUID(CCAP_CLSID_SystemDeviceEnum,       0x62be5d10, 0x60eb, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CCAP_CLSID_VideoInputDeviceCategory, 0x860bb310, 0x5d01, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CCAP_CLSID_FilterGraph,            0xe436ebb3, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_CLSID_CaptureGraphBuilder2,   0xbf87b6e1, 0x8c27, 0x11d0, 0xb3, 0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5);
DEFINE_GUID(CCAP_CLSID_SampleGrabber,          0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(CCAP_CLSID_NullRenderer,           0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);

// ============================================================================
// Media Types and Format Types
// ============================================================================
DEFINE_GUID(CCAP_MEDIATYPE_Video,          0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(CCAP_FORMAT_VideoInfo,         0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(CCAP_FORMAT_VideoInfo2,        0xf72a76a0, 0xeb0a, 0x11d0, 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);

// ============================================================================
// Pin Categories
// ============================================================================
DEFINE_GUID(CCAP_PIN_CATEGORY_CAPTURE,     0xfb6c4281, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(CCAP_PIN_CATEGORY_PREVIEW,     0xfb6c4282, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);

// ============================================================================
// Video Subtype GUIDs (MEDIASUBTYPE_*)
// ============================================================================
DEFINE_GUID(CCAP_MEDIASUBTYPE_None,  0xe436eb8e, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_MEDIASUBTYPE_MJPG,  0x47504A4D, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(CCAP_MEDIASUBTYPE_RGB24, 0xe436eb7d, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_MEDIASUBTYPE_RGB32, 0xe436eb7e, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_MEDIASUBTYPE_RGB555,0xe436eb7c, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CCAP_MEDIASUBTYPE_RGB565,0xe436eb7b, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);

// YUV format GUIDs - FourCC based GUIDs follow the pattern:
// {FOURCC-0000-0010-8000-00AA00389B71}
DEFINE_GUID(CCAP_MEDIASUBTYPE_NV12,  0x3231564E, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'NV12'
DEFINE_GUID(CCAP_MEDIASUBTYPE_I420,  0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'I420'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IYUV,  0x56555949, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IYUV'
DEFINE_GUID(CCAP_MEDIASUBTYPE_YUY2,  0x32595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'YUY2'
DEFINE_GUID(CCAP_MEDIASUBTYPE_YV12,  0x32315659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'YV12'
DEFINE_GUID(CCAP_MEDIASUBTYPE_UYVY,  0x59565955, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'UYVY'
DEFINE_GUID(CCAP_MEDIASUBTYPE_YUYV,  0x56595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'YUYV'
DEFINE_GUID(CCAP_MEDIASUBTYPE_YVYU,  0x55595659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'YVYU'
DEFINE_GUID(CCAP_MEDIASUBTYPE_YVU9,  0x39555659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'YVU9'
DEFINE_GUID(CCAP_MEDIASUBTYPE_Y411,  0x31313459, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'Y411'
DEFINE_GUID(CCAP_MEDIASUBTYPE_Y41P,  0x50313459, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'Y41P'
DEFINE_GUID(CCAP_MEDIASUBTYPE_CLJR,  0x524A4C43, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'CLJR'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IF09,  0x39304649, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IF09'
DEFINE_GUID(CCAP_MEDIASUBTYPE_CPLA,  0x414C5043, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'CPLA'
DEFINE_GUID(CCAP_MEDIASUBTYPE_AYUV,  0x56555941, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'AYUV'
DEFINE_GUID(CCAP_MEDIASUBTYPE_AI44,  0x34344941, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'AI44'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IA44,  0x34344149, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IA44'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IMC1,  0x31434D49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IMC1'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IMC2,  0x32434D49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IMC2'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IMC3,  0x33434D49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IMC3'
DEFINE_GUID(CCAP_MEDIASUBTYPE_IMC4,  0x34434D49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); // 'IMC4'

// clang-format on

// ============================================================================
// Aliases for compatibility - allows source code to use standard names
// These are defined as macros to reference the CCAP_ prefixed versions
// ============================================================================
#ifndef CCAP_NO_GUID_ALIASES

#define IID_ICreateDevEnum        CCAP_IID_ICreateDevEnum
#define IID_IBaseFilter           CCAP_IID_IBaseFilter
#define IID_IGraphBuilder         CCAP_IID_IGraphBuilder
#define IID_ICaptureGraphBuilder2 CCAP_IID_ICaptureGraphBuilder2
#define IID_IMediaControl         CCAP_IID_IMediaControl
#define IID_IMediaFilter          CCAP_IID_IMediaFilter
#define IID_IVideoWindow          CCAP_IID_IVideoWindow
#define IID_IAMStreamConfig       CCAP_IID_IAMStreamConfig
#define IID_IPropertyBag          CCAP_IID_IPropertyBag
#define IID_ISampleGrabber        CCAP_IID_ISampleGrabber

#define CLSID_SystemDeviceEnum        CCAP_CLSID_SystemDeviceEnum
#define CLSID_VideoInputDeviceCategory CCAP_CLSID_VideoInputDeviceCategory
#define CLSID_FilterGraph             CCAP_CLSID_FilterGraph
#define CLSID_CaptureGraphBuilder2    CCAP_CLSID_CaptureGraphBuilder2
#define CLSID_SampleGrabber           CCAP_CLSID_SampleGrabber
#define CLSID_NullRenderer            CCAP_CLSID_NullRenderer

#define MEDIATYPE_Video   CCAP_MEDIATYPE_Video
#define FORMAT_VideoInfo  CCAP_FORMAT_VideoInfo
#define FORMAT_VideoInfo2 CCAP_FORMAT_VideoInfo2

#define PIN_CATEGORY_CAPTURE CCAP_PIN_CATEGORY_CAPTURE
#define PIN_CATEGORY_PREVIEW CCAP_PIN_CATEGORY_PREVIEW

#define MEDIASUBTYPE_None   CCAP_MEDIASUBTYPE_None
#define MEDIASUBTYPE_MJPG   CCAP_MEDIASUBTYPE_MJPG
#define MEDIASUBTYPE_RGB24  CCAP_MEDIASUBTYPE_RGB24
#define MEDIASUBTYPE_RGB32  CCAP_MEDIASUBTYPE_RGB32
#define MEDIASUBTYPE_RGB555 CCAP_MEDIASUBTYPE_RGB555
#define MEDIASUBTYPE_RGB565 CCAP_MEDIASUBTYPE_RGB565
#define MEDIASUBTYPE_NV12   CCAP_MEDIASUBTYPE_NV12
#define MEDIASUBTYPE_I420   CCAP_MEDIASUBTYPE_I420
#define MEDIASUBTYPE_IYUV   CCAP_MEDIASUBTYPE_IYUV
#define MEDIASUBTYPE_YUY2   CCAP_MEDIASUBTYPE_YUY2
#define MEDIASUBTYPE_YV12   CCAP_MEDIASUBTYPE_YV12
#define MEDIASUBTYPE_UYVY   CCAP_MEDIASUBTYPE_UYVY
#define MEDIASUBTYPE_YUYV   CCAP_MEDIASUBTYPE_YUYV
#define MEDIASUBTYPE_YVYU   CCAP_MEDIASUBTYPE_YVYU
#define MEDIASUBTYPE_YVU9   CCAP_MEDIASUBTYPE_YVU9
#define MEDIASUBTYPE_Y411   CCAP_MEDIASUBTYPE_Y411
#define MEDIASUBTYPE_Y41P   CCAP_MEDIASUBTYPE_Y41P
#define MEDIASUBTYPE_CLJR   CCAP_MEDIASUBTYPE_CLJR
#define MEDIASUBTYPE_IF09   CCAP_MEDIASUBTYPE_IF09
#define MEDIASUBTYPE_CPLA   CCAP_MEDIASUBTYPE_CPLA
#define MEDIASUBTYPE_AYUV   CCAP_MEDIASUBTYPE_AYUV
#define MEDIASUBTYPE_AI44   CCAP_MEDIASUBTYPE_AI44
#define MEDIASUBTYPE_IA44   CCAP_MEDIASUBTYPE_IA44
#define MEDIASUBTYPE_IMC1   CCAP_MEDIASUBTYPE_IMC1
#define MEDIASUBTYPE_IMC2   CCAP_MEDIASUBTYPE_IMC2
#define MEDIASUBTYPE_IMC3   CCAP_MEDIASUBTYPE_IMC3
#define MEDIASUBTYPE_IMC4   CCAP_MEDIASUBTYPE_IMC4

#endif // CCAP_NO_GUID_ALIASES

#endif // _WIN32 || _MSC_VER

#endif // CCAP_DSHOW_GUIDS_H
