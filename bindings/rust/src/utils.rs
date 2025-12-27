use crate::error::{CcapError, Result};
use crate::frame::VideoFrame;
use crate::sys;
use crate::types::PixelFormat;
use std::ffi::CString;
use std::path::Path;

/// Utility functions
pub struct Utils;

impl Utils {
    /// Convert pixel format enum to string
    pub fn pixel_format_to_string(format: PixelFormat) -> Result<String> {
        let mut buffer = [0i8; 64];
        let result = unsafe {
            sys::ccap_pixel_format_to_string(format.to_c_enum(), buffer.as_mut_ptr(), buffer.len())
        };

        if result < 0 {
            return Err(CcapError::StringConversionError(
                "Unknown pixel format".to_string(),
            ));
        }

        let c_str = unsafe { std::ffi::CStr::from_ptr(buffer.as_ptr()) };
        c_str.to_str().map(|s| s.to_string()).map_err(|_| {
            CcapError::StringConversionError("Invalid pixel format string".to_string())
        })
    }

    /// Convert string to pixel format enum
    pub fn string_to_pixel_format(format_str: &str) -> Result<PixelFormat> {
        // This function doesn't exist in C API, we'll implement a simple mapping
        match format_str.to_lowercase().as_str() {
            "unknown" => Ok(PixelFormat::Unknown),
            "nv12" => Ok(PixelFormat::Nv12),
            "nv12f" => Ok(PixelFormat::Nv12F),
            "i420" => Ok(PixelFormat::I420),
            "i420f" => Ok(PixelFormat::I420F),
            "yuyv" => Ok(PixelFormat::Yuyv),
            "yuyvf" => Ok(PixelFormat::YuyvF),
            "uyvy" => Ok(PixelFormat::Uyvy),
            "uyvyf" => Ok(PixelFormat::UyvyF),
            "rgb24" => Ok(PixelFormat::Rgb24),
            "bgr24" => Ok(PixelFormat::Bgr24),
            "rgba32" => Ok(PixelFormat::Rgba32),
            "bgra32" => Ok(PixelFormat::Bgra32),
            _ => Err(CcapError::StringConversionError(
                "Unknown pixel format string".to_string(),
            )),
        }
    }

    /// Save frame as BMP file
    pub fn save_frame_as_bmp<P: AsRef<Path>>(frame: &VideoFrame, file_path: P) -> Result<()> {
        // This function doesn't exist in C API, we'll use the dump_frame_to_file instead
        Self::dump_frame_to_file(frame, file_path)?;
        Ok(())
    }

    /// Save a video frame to a file with automatic format detection
    pub fn dump_frame_to_file<P: AsRef<Path>>(
        frame: &VideoFrame,
        filename_no_suffix: P,
    ) -> Result<String> {
        let path_str = filename_no_suffix
            .as_ref()
            .to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        // First call to get required buffer size
        let buffer_size = unsafe {
            sys::ccap_dump_frame_to_file(frame.as_c_ptr(), c_path.as_ptr(), std::ptr::null_mut(), 0)
        };

        if buffer_size <= 0 {
            return Err(CcapError::FileOperationFailed(
                "Failed to dump frame to file".to_string(),
            ));
        }

        // Second call to get actual result
        let mut buffer = vec![0u8; buffer_size as usize];
        let result_len = unsafe {
            sys::ccap_dump_frame_to_file(
                frame.as_c_ptr(),
                c_path.as_ptr(),
                buffer.as_mut_ptr() as *mut i8,
                buffer.len(),
            )
        };

        if result_len <= 0 {
            return Err(CcapError::FileOperationFailed(
                "Failed to dump frame to file".to_string(),
            ));
        }

        // Convert to string
        buffer.truncate(result_len as usize);
        String::from_utf8(buffer)
            .map_err(|_| CcapError::StringConversionError("Invalid output path string".to_string()))
    }

    /// Save a video frame to directory with auto-generated filename
    pub fn dump_frame_to_directory<P: AsRef<Path>>(
        frame: &VideoFrame,
        directory: P,
    ) -> Result<String> {
        let dir_str = directory.as_ref().to_str().ok_or_else(|| {
            CcapError::StringConversionError("Invalid directory path".to_string())
        })?;

        let c_dir = CString::new(dir_str)
            .map_err(|_| CcapError::StringConversionError("Invalid directory path".to_string()))?;

        // First call to get required buffer size
        let buffer_size = unsafe {
            sys::ccap_dump_frame_to_directory(
                frame.as_c_ptr(),
                c_dir.as_ptr(),
                std::ptr::null_mut(),
                0,
            )
        };

        if buffer_size <= 0 {
            return Err(CcapError::FileOperationFailed(
                "Failed to dump frame to directory".to_string(),
            ));
        }

        // Second call to get actual result
        let mut buffer = vec![0u8; buffer_size as usize];
        let result_len = unsafe {
            sys::ccap_dump_frame_to_directory(
                frame.as_c_ptr(),
                c_dir.as_ptr(),
                buffer.as_mut_ptr() as *mut i8,
                buffer.len(),
            )
        };

        if result_len <= 0 {
            return Err(CcapError::FileOperationFailed(
                "Failed to dump frame to directory".to_string(),
            ));
        }

        // Convert to string
        buffer.truncate(result_len as usize);
        String::from_utf8(buffer)
            .map_err(|_| CcapError::StringConversionError("Invalid output path string".to_string()))
    }

    /// Save RGB data as BMP file (generic version)
    #[allow(clippy::too_many_arguments)]
    pub fn save_rgb_data_as_bmp<P: AsRef<Path>>(
        filename: P,
        data: &[u8],
        width: u32,
        stride: u32,
        height: u32,
        is_bgr: bool,
        has_alpha: bool,
        is_top_to_bottom: bool,
    ) -> Result<()> {
        let path_str = filename
            .as_ref()
            .to_str()
            .ok_or_else(|| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let c_path = CString::new(path_str)
            .map_err(|_| CcapError::StringConversionError("Invalid file path".to_string()))?;

        let success = unsafe {
            sys::ccap_save_rgb_data_as_bmp(
                c_path.as_ptr(),
                data.as_ptr(),
                width,
                stride,
                height,
                is_bgr,
                has_alpha,
                is_top_to_bottom,
            )
        };

        if success {
            Ok(())
        } else {
            Err(CcapError::FileOperationFailed(
                "Failed to save RGB data as BMP".to_string(),
            ))
        }
    }

    /// Interactive camera selection helper
    pub fn select_camera(devices: &[String]) -> Result<usize> {
        if devices.is_empty() {
            return Err(CcapError::DeviceNotFound);
        }

        if devices.len() == 1 {
            println!("Using the only available device: {}", devices[0]);
            return Ok(0);
        }

        println!("Multiple devices found, please select one:");
        for (i, device) in devices.iter().enumerate() {
            println!("  {}: {}", i, device);
        }

        print!("Enter the index of the device you want to use: ");
        use std::io::{self, Write};
        io::stdout().flush().unwrap();

        let mut input = String::new();
        io::stdin()
            .read_line(&mut input)
            .map_err(|e| CcapError::InvalidParameter(format!("Failed to read input: {}", e)))?;

        let selected_index = input.trim().parse::<usize>().unwrap_or(0);

        if selected_index >= devices.len() {
            println!("Invalid index, using the first device: {}", devices[0]);
            Ok(0)
        } else {
            println!("Using device: {}", devices[selected_index]);
            Ok(selected_index)
        }
    }

    /// Set log level
    pub fn set_log_level(level: LogLevel) {
        unsafe {
            sys::ccap_set_log_level(level.to_c_enum());
        }
    }
}

/// Log level enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    /// No log output
    None,
    /// Error log level
    Error,
    /// Warning log level
    Warning,
    /// Info log level
    Info,
    /// Verbose log level
    Verbose,
}

impl LogLevel {
    /// Convert log level to C enum
    pub fn to_c_enum(self) -> sys::CcapLogLevel {
        match self {
            LogLevel::None => sys::CcapLogLevel_CCAP_LOG_LEVEL_NONE,
            LogLevel::Error => sys::CcapLogLevel_CCAP_LOG_LEVEL_ERROR,
            LogLevel::Warning => sys::CcapLogLevel_CCAP_LOG_LEVEL_WARNING,
            LogLevel::Info => sys::CcapLogLevel_CCAP_LOG_LEVEL_INFO,
            LogLevel::Verbose => sys::CcapLogLevel_CCAP_LOG_LEVEL_VERBOSE,
        }
    }
}
