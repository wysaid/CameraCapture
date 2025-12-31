use crate::error::{CcapError, Result};
use crate::sys;
use crate::types::ColorConversionBackend;
use std::os::raw::c_int;

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
        let success = unsafe { sys::ccap_convert_set_backend(backend.to_c_enum()) };

        if success {
            Ok(())
        } else {
            Err(CcapError::BackendSetFailed)
        }
    }

    /// Check if AVX2 is available
    pub fn has_avx2() -> bool {
        unsafe { sys::ccap_convert_has_avx2() }
    }

    /// Check if Apple Accelerate is available
    pub fn has_apple_accelerate() -> bool {
        unsafe { sys::ccap_convert_has_apple_accelerate() }
    }

    /// Check if NEON is available
    pub fn has_neon() -> bool {
        unsafe { sys::ccap_convert_has_neon() }
    }

    /// Convert YUYV to RGB24
    pub fn yuyv_to_rgb24(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_yuyv_to_rgb24(
                src_data.as_ptr(),
                src_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }

    /// Convert YUYV to BGR24
    pub fn yuyv_to_bgr24(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_yuyv_to_bgr24(
                src_data.as_ptr(),
                src_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }

    /// Convert RGB to BGR
    pub fn rgb_to_bgr(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_rgb_to_bgr(
                src_data.as_ptr(),
                src_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
            )
        };

        Ok(dst_data)
    }

    /// Convert BGR to RGB
    pub fn bgr_to_rgb(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_bgr_to_rgb(
                src_data.as_ptr(),
                src_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
            )
        };

        Ok(dst_data)
    }

    /// Convert NV12 to RGB24
    pub fn nv12_to_rgb24(
        y_data: &[u8],
        y_stride: usize,
        uv_data: &[u8],
        uv_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_nv12_to_rgb24(
                y_data.as_ptr(),
                y_stride as c_int,
                uv_data.as_ptr(),
                uv_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }

    /// Convert NV12 to BGR24
    pub fn nv12_to_bgr24(
        y_data: &[u8],
        y_stride: usize,
        uv_data: &[u8],
        uv_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_nv12_to_bgr24(
                y_data.as_ptr(),
                y_stride as c_int,
                uv_data.as_ptr(),
                uv_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }

    /// Convert I420 to RGB24
    #[allow(clippy::too_many_arguments)]
    pub fn i420_to_rgb24(
        y_data: &[u8],
        y_stride: usize,
        u_data: &[u8],
        u_stride: usize,
        v_data: &[u8],
        v_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_i420_to_rgb24(
                y_data.as_ptr(),
                y_stride as c_int,
                u_data.as_ptr(),
                u_stride as c_int,
                v_data.as_ptr(),
                v_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }

    /// Convert I420 to BGR24
    #[allow(clippy::too_many_arguments)]
    pub fn i420_to_bgr24(
        y_data: &[u8],
        y_stride: usize,
        u_data: &[u8],
        u_stride: usize,
        v_data: &[u8],
        v_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let dst_stride = (width * 3) as usize;
        let dst_size = dst_stride * height as usize;
        let mut dst_data = vec![0u8; dst_size];

        unsafe {
            sys::ccap_convert_i420_to_bgr24(
                y_data.as_ptr(),
                y_stride as c_int,
                u_data.as_ptr(),
                u_stride as c_int,
                v_data.as_ptr(),
                v_stride as c_int,
                dst_data.as_mut_ptr(),
                dst_stride as c_int,
                width as c_int,
                height as c_int,
                sys::CcapConvertFlag_CCAP_CONVERT_FLAG_DEFAULT,
            )
        };

        Ok(dst_data)
    }
}
