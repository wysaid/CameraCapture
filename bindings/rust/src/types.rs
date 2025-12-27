use crate::sys;

/// Pixel format enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PixelFormat {
    /// Unknown pixel format
    Unknown,
    /// NV12 pixel format
    Nv12,
    /// NV12F pixel format
    Nv12F,
    /// I420 pixel format
    I420,
    /// I420F pixel format
    I420F,
    /// YUYV pixel format
    Yuyv,
    /// YUYV flipped pixel format
    YuyvF,
    /// UYVY pixel format
    Uyvy,
    /// UYVY flipped pixel format
    UyvyF,
    /// RGB24 pixel format
    Rgb24,
    /// BGR24 pixel format
    Bgr24,
    /// RGBA32 pixel format
    Rgba32,
    /// BGRA32 pixel format
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
    /// Convert pixel format to C enum
    pub fn to_c_enum(self) -> sys::CcapPixelFormat {
        self.into()
    }

    /// Create pixel format from C enum
    pub fn from_c_enum(format: sys::CcapPixelFormat) -> Self {
        format.into()
    }

    /// Get string representation of pixel format
    pub fn as_str(self) -> &'static str {
        match self {
            PixelFormat::Unknown => "Unknown",
            PixelFormat::Nv12 => "NV12",
            PixelFormat::Nv12F => "NV12F",
            PixelFormat::I420 => "I420",
            PixelFormat::I420F => "I420F",
            PixelFormat::Yuyv => "YUYV",
            PixelFormat::YuyvF => "YUYV_F",
            PixelFormat::Uyvy => "UYVY",
            PixelFormat::UyvyF => "UYVY_F",
            PixelFormat::Rgb24 => "RGB24",
            PixelFormat::Bgr24 => "BGR24",
            PixelFormat::Rgba32 => "RGBA32",
            PixelFormat::Bgra32 => "BGRA32",
        }
    }
}

impl From<PixelFormat> for sys::CcapPixelFormat {
    fn from(val: PixelFormat) -> Self {
        match val {
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
    /// Top to bottom orientation
    TopToBottom,
    /// Bottom to top orientation
    BottomToTop,
}

impl From<sys::CcapFrameOrientation> for FrameOrientation {
    fn from(orientation: sys::CcapFrameOrientation) -> Self {
        match orientation {
            sys::CcapFrameOrientation_CCAP_FRAME_ORIENTATION_TOP_TO_BOTTOM => {
                FrameOrientation::TopToBottom
            }
            sys::CcapFrameOrientation_CCAP_FRAME_ORIENTATION_BOTTOM_TO_TOP => {
                FrameOrientation::BottomToTop
            }
            _ => FrameOrientation::TopToBottom,
        }
    }
}

/// Camera property enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PropertyName {
    /// Width property
    Width,
    /// Height property
    Height,
    /// Frame rate property
    FrameRate,
    /// Internal pixel format property
    PixelFormatInternal,
    /// Output pixel format property
    PixelFormatOutput,
    /// Frame orientation property
    FrameOrientation,
}

impl PropertyName {
    /// Convert property name to C enum
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
            PropertyName::PixelFormatInternal => {
                sys::CcapPropertyName_CCAP_PROPERTY_PIXEL_FORMAT_INTERNAL
            }
            PropertyName::PixelFormatOutput => {
                sys::CcapPropertyName_CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT
            }
            PropertyName::FrameOrientation => sys::CcapPropertyName_CCAP_PROPERTY_FRAME_ORIENTATION,
        }
    }
}

/// Color conversion backend enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ColorConversionBackend {
    /// CPU backend
    Cpu,
    /// AVX2 backend
    Avx2,
    /// NEON backend
    Neon,
    /// Apple Accelerate backend
    Accelerate,
}

impl ColorConversionBackend {
    /// Convert backend to C enum
    pub fn to_c_enum(self) -> sys::CcapConvertBackend {
        match self {
            ColorConversionBackend::Cpu => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_CPU,
            ColorConversionBackend::Avx2 => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_AVX2,
            ColorConversionBackend::Neon => sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_NEON,
            ColorConversionBackend::Accelerate => {
                sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_APPLE_ACCELERATE
            }
        }
    }

    /// Create backend from C enum
    pub fn from_c_enum(backend: sys::CcapConvertBackend) -> Self {
        match backend {
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_CPU => ColorConversionBackend::Cpu,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_AVX2 => ColorConversionBackend::Avx2,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_NEON => ColorConversionBackend::Neon,
            sys::CcapConvertBackend_CCAP_CONVERT_BACKEND_APPLE_ACCELERATE => {
                ColorConversionBackend::Accelerate
            }
            _ => ColorConversionBackend::Cpu,
        }
    }
}

/// Resolution structure
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Resolution {
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
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
