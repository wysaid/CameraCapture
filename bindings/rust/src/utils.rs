use crate::error::{CcapError, Result};
use crate::types::PixelFormat;
use crate::frame::VideoFrame;
use crate::sys;
use std::ffi::{CStr, CString};
use std::path::Path;

/// Utility functions
pub struct Utils;

impl Utils {
    /// Convert pixel format enum to string
    pub fn pixel_format_to_string(format: PixelFormat) -> Result<String> {
        let format_ptr = unsafe { 
            sys::ccap_utils_pixel_format_to_string(format.to_c_enum()) 
        };
        
        if format_ptr.is_null() {
            return Err(CcapError::StringConversionError("Unknown pixel format".to_string()));
        }

        let format_cstr = unsafe { CStr::from_ptr(format_ptr) };
        format_cstr
            .to_str()
            .map(|s| s.to_string())
            .map_err(|_| CcapError::StringConversionError("Invalid pixel format string".to_string()))
    }

    /// Convert string to pixel format enum
    pub fn string_to_pixel_format(format_str: &str) -> Result<PixelFormat> {
        let c_format_str = CString::new(format_str)
            .map_err(|_| CcapError::StringConversionError("Invalid format string".to_string()))?;
        
        let format_value = unsafe { 
            sys::ccap_utils_string_to_pixel_format(c_format_str.as_ptr()) 
        };
        
        if format_value == sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_UNKNOWN {
            Err(CcapError::StringConversionError("Unknown pixel format string".to_string()))
        } else {
            Ok(PixelFormat::from_c_enum(format_value))
        }
    }

    /// Save frame as BMP file
    pub fn save_frame_as_bmp<P: AsRef<Path>>(frame: &VideoFrame, file_path: P) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_frame_as_bmp(frame.as_c_ptr(), c_path.as_ptr())
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save BMP file".to_string()))
        }
    }

    /// Save RGB24 data as BMP file
    pub fn save_rgb24_as_bmp<P: AsRef<Path>>(
        data: &[u8],
        width: u32,
        height: u32,
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_rgb24_as_bmp(
                data.as_ptr(),
                width,
                height,
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save RGB24 BMP file".to_string()))
        }
    }

    /// Save BGR24 data as BMP file
    pub fn save_bgr24_as_bmp<P: AsRef<Path>>(
        data: &[u8],
        width: u32,
        height: u32,
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_bgr24_as_bmp(
                data.as_ptr(),
                width,
                height,
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save BGR24 BMP file".to_string()))
        }
    }

    /// Save YUYV422 data as BMP file
    pub fn save_yuyv422_as_bmp<P: AsRef<Path>>(
        data: &[u8],
        width: u32,
        height: u32,
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_yuyv422_as_bmp(
                data.as_ptr(),
                width,
                height,
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save YUYV422 BMP file".to_string()))
        }
    }

    /// Save MJPEG data as JPEG file
    pub fn save_mjpeg_as_jpeg<P: AsRef<Path>>(
        data: &[u8],
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_mjpeg_as_jpeg(
                data.as_ptr(),
                data.len(),
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save MJPEG file".to_string()))
        }
    }

    /// Save NV12 data as BMP file
    pub fn save_nv12_as_bmp<P: AsRef<Path>>(
        y_data: &[u8],
        uv_data: &[u8],
        width: u32,
        height: u32,
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_nv12_as_bmp(
                y_data.as_ptr(),
                uv_data.as_ptr(),
                width,
                height,
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save NV12 BMP file".to_string()))
        }
    }

    /// Save YV12 data as BMP file  
    pub fn save_yv12_as_bmp<P: AsRef<Path>>(
        y_data: &[u8],
        u_data: &[u8],
        v_data: &[u8],
        width: u32,
        height: u32,
        file_path: P,
    ) -> Result<()> {
        let path_str = file_path.as_ref().to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;
        
        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_utils_save_yv12_as_bmp(
                y_data.as_ptr(),
                u_data.as_ptr(),
                v_data.as_ptr(),
                width,
                height,
                c_path.as_ptr(),
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed("Failed to save YV12 BMP file".to_string()))
        }
    }
}
