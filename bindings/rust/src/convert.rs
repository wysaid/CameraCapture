use crate::error::{CcapError, Result};
use crate::sys;
use crate::types::ColorConversionBackend;
use std::os::raw::c_int;

/// Color conversion utilities
pub struct Convert;

/// Validate that the input buffer has sufficient size
fn validate_buffer_size(data: &[u8], required: usize, name: &str) -> Result<()> {
    if data.len() < required {
        return Err(CcapError::InvalidParameter(format!(
            "{} buffer too small: got {} bytes, need at least {} bytes",
            name,
            data.len(),
            required
        )));
    }
    Ok(())
}

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if `src_data` is too small for the given dimensions.
    pub fn yuyv_to_rgb24(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let required = src_stride * height as usize;
        validate_buffer_size(src_data, required, "YUYV source")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if `src_data` is too small for the given dimensions.
    pub fn yuyv_to_bgr24(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let required = src_stride * height as usize;
        validate_buffer_size(src_data, required, "YUYV source")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if `src_data` is too small for the given dimensions.
    pub fn rgb_to_bgr(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let required = src_stride * height as usize;
        validate_buffer_size(src_data, required, "RGB source")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if `src_data` is too small for the given dimensions.
    pub fn bgr_to_rgb(
        src_data: &[u8],
        src_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let required = src_stride * height as usize;
        validate_buffer_size(src_data, required, "BGR source")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if buffers are too small for the given dimensions.
    pub fn nv12_to_rgb24(
        y_data: &[u8],
        y_stride: usize,
        uv_data: &[u8],
        uv_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let y_required = y_stride * height as usize;
        let uv_required = uv_stride * ((height as usize + 1) / 2);
        validate_buffer_size(y_data, y_required, "NV12 Y plane")?;
        validate_buffer_size(uv_data, uv_required, "NV12 UV plane")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if buffers are too small for the given dimensions.
    pub fn nv12_to_bgr24(
        y_data: &[u8],
        y_stride: usize,
        uv_data: &[u8],
        uv_stride: usize,
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let y_required = y_stride * height as usize;
        let uv_required = uv_stride * ((height as usize + 1) / 2);
        validate_buffer_size(y_data, y_required, "NV12 Y plane")?;
        validate_buffer_size(uv_data, uv_required, "NV12 UV plane")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if buffers are too small for the given dimensions.
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
        let y_required = y_stride * height as usize;
        let uv_height = (height as usize + 1) / 2;
        let u_required = u_stride * uv_height;
        let v_required = v_stride * uv_height;
        validate_buffer_size(y_data, y_required, "I420 Y plane")?;
        validate_buffer_size(u_data, u_required, "I420 U plane")?;
        validate_buffer_size(v_data, v_required, "I420 V plane")?;

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
    ///
    /// # Errors
    ///
    /// Returns `CcapError::InvalidParameter` if buffers are too small for the given dimensions.
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
        let y_required = y_stride * height as usize;
        let uv_height = (height as usize + 1) / 2;
        let u_required = u_stride * uv_height;
        let v_required = v_stride * uv_height;
        validate_buffer_size(y_data, y_required, "I420 Y plane")?;
        validate_buffer_size(u_data, u_required, "I420 U plane")?;
        validate_buffer_size(v_data, v_required, "I420 V plane")?;

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_detection() {
        // Should be able to get current backend without panic
        let backend = Convert::backend();
        println!("Current backend: {:?}", backend);
    }

    #[test]
    fn test_simd_availability() {
        // These should return booleans without panic
        let has_avx2 = Convert::has_avx2();
        let has_neon = Convert::has_neon();
        let has_accelerate = Convert::has_apple_accelerate();

        println!(
            "AVX2: {}, NEON: {}, Accelerate: {}",
            has_avx2, has_neon, has_accelerate
        );

        // At most one SIMD backend should be available (platform-dependent)
        // On x86: AVX2 may be available
        // On ARM: NEON may be available
        // On macOS: Accelerate may be available
    }

    #[test]
    fn test_rgb_bgr_conversion() {
        let width = 4u32;
        let height = 4u32;
        let stride = (width * 3) as usize;

        // Create a simple RGB pattern: red, green, blue, white
        let mut rgb_data = vec![0u8; stride * height as usize];
        for y in 0..height as usize {
            for x in 0..width as usize {
                let offset = y * stride + x * 3;
                match (x + y) % 4 {
                    0 => {
                        rgb_data[offset] = 255;
                        rgb_data[offset + 1] = 0;
                        rgb_data[offset + 2] = 0;
                    } // Red
                    1 => {
                        rgb_data[offset] = 0;
                        rgb_data[offset + 1] = 255;
                        rgb_data[offset + 2] = 0;
                    } // Green
                    2 => {
                        rgb_data[offset] = 0;
                        rgb_data[offset + 1] = 0;
                        rgb_data[offset + 2] = 255;
                    } // Blue
                    _ => {
                        rgb_data[offset] = 255;
                        rgb_data[offset + 1] = 255;
                        rgb_data[offset + 2] = 255;
                    } // White
                }
            }
        }

        // Convert RGB to BGR
        let bgr_data = Convert::rgb_to_bgr(&rgb_data, stride, width, height).unwrap();
        assert_eq!(bgr_data.len(), rgb_data.len());

        // Verify R and B channels are swapped
        for y in 0..height as usize {
            for x in 0..width as usize {
                let offset = y * stride + x * 3;
                assert_eq!(
                    rgb_data[offset],
                    bgr_data[offset + 2],
                    "R->B at ({}, {})",
                    x,
                    y
                );
                assert_eq!(
                    rgb_data[offset + 1],
                    bgr_data[offset + 1],
                    "G==G at ({}, {})",
                    x,
                    y
                );
                assert_eq!(
                    rgb_data[offset + 2],
                    bgr_data[offset],
                    "B->R at ({}, {})",
                    x,
                    y
                );
            }
        }

        // Convert back: BGR to RGB should restore original
        let restored_rgb = Convert::bgr_to_rgb(&bgr_data, stride, width, height).unwrap();
        assert_eq!(
            restored_rgb, rgb_data,
            "Round-trip RGB->BGR->RGB should be identical"
        );
    }

    #[test]
    fn test_nv12_to_rgb_basic() {
        let width = 16u32;
        let height = 16u32;
        let y_stride = width as usize;
        let uv_stride = width as usize;

        // Create neutral gray NV12 data (Y=128, U=128, V=128 -> gray in RGB)
        let y_data = vec![128u8; y_stride * height as usize];
        let uv_data = vec![128u8; uv_stride * (height as usize / 2)];

        let rgb_data =
            Convert::nv12_to_rgb24(&y_data, y_stride, &uv_data, uv_stride, width, height).unwrap();

        // Verify output size
        let expected_size = (width * 3) as usize * height as usize;
        assert_eq!(rgb_data.len(), expected_size);

        // All pixels should be approximately gray (128, 128, 128) with some YUV rounding tolerance
        for pixel in rgb_data.chunks(3) {
            assert!(
                pixel[0] >= 100 && pixel[0] <= 156,
                "R should be near 128, got {}",
                pixel[0]
            );
            assert!(
                pixel[1] >= 100 && pixel[1] <= 156,
                "G should be near 128, got {}",
                pixel[1]
            );
            assert!(
                pixel[2] >= 100 && pixel[2] <= 156,
                "B should be near 128, got {}",
                pixel[2]
            );
        }
    }

    #[test]
    fn test_nv12_to_bgr_basic() {
        let width = 16u32;
        let height = 16u32;
        let y_stride = width as usize;
        let uv_stride = width as usize;

        let y_data = vec![128u8; y_stride * height as usize];
        let uv_data = vec![128u8; uv_stride * (height as usize / 2)];

        let bgr_data =
            Convert::nv12_to_bgr24(&y_data, y_stride, &uv_data, uv_stride, width, height).unwrap();

        let expected_size = (width * 3) as usize * height as usize;
        assert_eq!(bgr_data.len(), expected_size);
    }

    #[test]
    fn test_i420_to_rgb_basic() {
        let width = 16u32;
        let height = 16u32;
        let y_stride = width as usize;
        let u_stride = (width / 2) as usize;
        let v_stride = (width / 2) as usize;

        let y_data = vec![128u8; y_stride * height as usize];
        let u_data = vec![128u8; u_stride * (height as usize / 2)];
        let v_data = vec![128u8; v_stride * (height as usize / 2)];

        let rgb_data = Convert::i420_to_rgb24(
            &y_data, y_stride, &u_data, u_stride, &v_data, v_stride, width, height,
        )
        .unwrap();

        let expected_size = (width * 3) as usize * height as usize;
        assert_eq!(rgb_data.len(), expected_size);
    }

    #[test]
    fn test_yuyv_to_rgb_basic() {
        let width = 16u32;
        let height = 16u32;
        let stride = (width * 2) as usize; // YUYV: 2 bytes per pixel

        // Create neutral YUYV data (Y=128, U=128, V=128)
        let mut yuyv_data = vec![0u8; stride * height as usize];
        for i in 0..(stride * height as usize / 4) {
            yuyv_data[i * 4] = 128; // Y0
            yuyv_data[i * 4 + 1] = 128; // U
            yuyv_data[i * 4 + 2] = 128; // Y1
            yuyv_data[i * 4 + 3] = 128; // V
        }

        let rgb_data = Convert::yuyv_to_rgb24(&yuyv_data, stride, width, height).unwrap();

        let expected_size = (width * 3) as usize * height as usize;
        assert_eq!(rgb_data.len(), expected_size);
    }

    #[test]
    fn test_buffer_too_small_error() {
        let width = 16u32;
        let height = 16u32;

        // Provide a buffer that's too small
        let small_buffer = vec![0u8; 10];

        let result = Convert::yuyv_to_rgb24(&small_buffer, width as usize * 2, width, height);
        assert!(result.is_err());

        if let Err(CcapError::InvalidParameter(msg)) = result {
            assert!(
                msg.contains("too small"),
                "Error message should mention 'too small'"
            );
        } else {
            panic!("Expected InvalidParameter error");
        }
    }

    #[test]
    fn test_nv12_buffer_validation() {
        let width = 16u32;
        let height = 16u32;
        let y_stride = width as usize;
        let uv_stride = width as usize;

        // Y plane too small
        let small_y = vec![0u8; 10];
        let uv_data = vec![128u8; uv_stride * (height as usize / 2)];
        let result = Convert::nv12_to_rgb24(&small_y, y_stride, &uv_data, uv_stride, width, height);
        assert!(result.is_err());

        // UV plane too small
        let y_data = vec![128u8; y_stride * height as usize];
        let small_uv = vec![0u8; 10];
        let result = Convert::nv12_to_rgb24(&y_data, y_stride, &small_uv, uv_stride, width, height);
        assert!(result.is_err());
    }
}
