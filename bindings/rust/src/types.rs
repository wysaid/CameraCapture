use crate::sys;

/// Pixel format enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PixelFormat {
    Unknown,
    Nv12,
    Nv12F,
    I420,
    I420F,
    Yuyv,
    YuyvF,
    Uyvy,
    UyvyF,
    Rgb24,
    Bgr24,
    Rgba32,
    Bgra32,
}

impl From<sys::CcapPixelFormat> for PixelFormat {
    fn from(format: sys::CcapPixelFormat) -> Self {
        match format {
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UNKNOWN => PixelFormat::Unknown,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_NV12 => PixelFormat::Nv12,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_NV12F => PixelFormat::Nv12F,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_I420 => PixelFormat::I420,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_I420F => PixelFormat::I420F,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_YUYV => PixelFormat::Yuyv,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_YUYV_F => PixelFormat::YuyvF,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UYVY => PixelFormat::Uyvy,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UYVY_F => PixelFormat::UyvyF,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_RGB24 => PixelFormat::Rgb24,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_BGR24 => PixelFormat::Bgr24,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_RGBA32 => PixelFormat::Rgba32,
            sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_BGRA32 => PixelFormat::Bgra32,
            _ => PixelFormat::Unknown,
        }
    }
}

impl PixelFormat {
    pub fn to_c_enum(self) -> sys::CcapPixelFormat {
        self.into()
    }

    pub fn from_c_enum(format: sys::CcapPixelFormat) -> Self {
        format.into()
    }
}

impl Into<sys::CcapPixelFormat> for PixelFormat {
    fn into(self) -> sys::CcapPixelFormat {
        match self {
            PixelFormat::Unknown => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UNKNOWN,
            PixelFormat::Nv12 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_NV12,
            PixelFormat::Nv12F => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_NV12F,
            PixelFormat::I420 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_I420,
            PixelFormat::I420F => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_I420F,
            PixelFormat::Yuyv => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_YUYV,
            PixelFormat::YuyvF => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_YUYV_F,
            PixelFormat::Uyvy => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UYVY,
            PixelFormat::UyvyF => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UYVY_F,
            PixelFormat::Rgb24 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_RGB24,
            PixelFormat::Bgr24 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_BGR24,
            PixelFormat::Rgba32 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_RGBA32,
            PixelFormat::Bgra32 => sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_BGRA32,
        }
    }
}

/// Frame orientation enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameOrientation {
    TopToBottom,
    BottomToTop,
}

impl From<sys::CcapFrameOrientation> for FrameOrientation {
    fn from(orientation: sys::CcapFrameOrientation) -> Self {
        match orientation {
            sys::CcapFrameOrientation_CCAP_FRAME_ORIENTATION_TOP_TO_BOTTOM => FrameOrientation::TopToBottom,
            sys::CcapFrameOrientation_CCAP_FRAME_ORIENTATION_BOTTOM_TO_TOP => FrameOrientation::BottomToTop,
            _ => FrameOrientation::TopToBottom,
        }
    }
}

/// Camera property enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PropertyName {
    Width,
    Height,
    FrameRate,
    PixelFormatInternal,
    PixelFormatOutput,
    FrameOrientation,
}

impl PropertyName {
    pub fn to_c_enum(self) -> sys::CcapPropertyName {
        self.into()
    }
}

impl From<PropertyName> for sys::CcapPropertyName {
    fn from(prop: PropertyName) -> Self {
        match prop {
            PropertyName::Width => sys::CcapPropertyName_CCAP_PROPERTY_WIDTH,
            PropertyName::Height => sys::CcapPropertyName_CCAP_PROPERTY_HEIGHT,
            PropertyName::FrameRate => sys::CcapPropertyName_CCAP_PROPERTY_FRAME_RATE,
            PropertyName::PixelFormatInternal => sys::CcapPropertyName_CCAP_PROPERTY_PIXEL_FORMAT_INTERNAL,
            PropertyName::PixelFormatOutput => sys::CcapPropertyName_CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT,
            PropertyName::FrameOrientation => sys::CcapPropertyName_CCAP_PROPERTY_FRAME_ORIENTATION,
        }
    }
}

/// Color conversion backend enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ColorConversionBackend {
    Cpu,
    Avx2,
    Neon,
    Accelerate,
}

impl ColorConversionBackend {
    pub fn to_c_enum(self) -> sys::CcapConvertBackend {
        match self {
            ColorConversionBackend::Cpu => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_CPU,
            ColorConversionBackend::Avx2 => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_AVX2,
            ColorConversionBackend::Neon => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_NEON,
            ColorConversionBackend::Accelerate => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_APPLE_ACCELERATE,
        }
    }

    pub fn from_c_enum(backend: sys::CcapConvertBackend) -> Self {
        match backend {
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_CPU => ColorConversionBackend::Cpu,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_AVX2 => ColorConversionBackend::Avx2,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_NEON => ColorConversionBackend::Neon,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_APPLE_ACCELERATE => ColorConversionBackend::Accelerate,
            _ => ColorConversionBackend::Cpu,
        }
    }
}

/// Resolution structure
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Resolution {
    pub width: u32,
    pub height: u32,
}

impl From<sys::CcapResolution> for Resolution {
    fn from(res: sys::CcapResolution) -> Self {
        Resolution {
            width: res.width,
            height: res.height,
        }
    }
}
