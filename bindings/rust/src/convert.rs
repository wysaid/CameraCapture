use crate::error::{CcapError, Result};
use crate::types::{PixelFormat, ColorConversionBackend};
use crate::frame::VideoFrame;
use crate::sys;
use std::ffi::{CStr, CString};

/// Color conversion utilities
pub struct Convert;

impl Convert {
    /// Get current color conversion backend
    pub fn backend() -> ColorConversionBackend {
        let backend = unsafe { sys::ccap_convert_get_backend() };
        ColorConversionBackend::from_c_enum(backend)
    }

    /// Set color conversion backend
    pub fn set_backend(backend: ColorConversionBackend) -> Result<()> {
        let success = unsafe {
            sys::ccap_convert_set_backend(backend.to_c_enum())
        };
        
        if success {
            Ok(())
        } else {
            Err(CcapError::BackendSetFailed)
        }
    }

    /// Get backend name as string
    pub fn backend_name() -> Result<String> {
        let backend_ptr = unsafe { sys::ccap_convert_get_backend_name() };
        if backend_ptr.is_null() {
            return Err(CcapError::InternalError("Failed to get backend name".to_string()));
        }

        let backend_cstr = unsafe { CStr::from_ptr(backend_ptr) };
        backend_cstr
            .to_str()
            .map(|s| s.to_string())
            .map_err(|_| CcapError::StringConversionError("Invalid backend name".to_string()))
    }

    /// Check if backend is available
    pub fn is_backend_available(backend: ColorConversionBackend) -> bool {
        unsafe { sys::ccap_convert_is_backend_available(backend.to_c_enum()) }
    }

    /// Convert frame to different pixel format
    pub fn convert_frame(
        src_frame: &VideoFrame,
        dst_format: PixelFormat,
    ) -> Result<VideoFrame> {
        let dst_ptr = unsafe {
            sys::ccap_convert_frame(src_frame.as_c_ptr(), dst_format.to_c_enum())
        };

        if dst_ptr.is_null() {
            Err(CcapError::ConversionFailed)
        } else {
            Ok(VideoFrame::from_c_ptr(dst_ptr))
        }
    }

    /// Convert YUYV422 to RGB24
    pub fn yuyv422_to_rgb24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_yuyv422_to_rgb24(
                src_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert YUYV422 to BGR24
    pub fn yuyv422_to_bgr24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_yuyv422_to_bgr24(
                src_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert RGB24 to BGR24
    pub fn rgb24_to_bgr24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_rgb24_to_bgr24(
                src_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert BGR24 to RGB24
    pub fn bgr24_to_rgb24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_bgr24_to_rgb24(
                src_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert MJPEG to RGB24
    pub fn mjpeg_to_rgb24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_mjpeg_to_rgb24(
                src_data.as_ptr(),
                src_data.len(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert MJPEG to BGR24
    pub fn mjpeg_to_bgr24(
        src_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_mjpeg_to_bgr24(
                src_data.as_ptr(),
                src_data.len(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert NV12 to RGB24
    pub fn nv12_to_rgb24(
        y_data: &[u8],
        uv_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_nv12_to_rgb24(
                y_data.as_ptr(),
                uv_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert NV12 to BGR24
    pub fn nv12_to_bgr24(
        y_data: &[u8],
        uv_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_nv12_to_bgr24(
                y_data.as_ptr(),
                uv_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert YV12 to RGB24
    pub fn yv12_to_rgb24(
        y_data: &[u8],
        u_data: &[u8],
        v_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_yv12_to_rgb24(
                y_data.as_ptr(),
                u_data.as_ptr(),
                v_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }

    /// Convert YV12 to BGR24
    pub fn yv12_to_bgr24(
        y_data: &[u8],
        u_data: &[u8],
        v_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_size = (width * height * 3) as usize;
        let mut dst_data = vec![0u8; dst_size];

        let success = unsafe {
            sys::ccap_convert_yv12_to_bgr24(
                y_data.as_ptr(),
                u_data.as_ptr(),
                v_data.as_ptr(),
                dst_data.as_mut_ptr(),
                width,
                height,
            )
        };

        if success {
            Ok(dst_data)
        } else {
            Err(CcapError::ConversionFailed)
        }
    }
}
