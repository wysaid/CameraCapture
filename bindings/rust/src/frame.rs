use crate::{error::CcapError, sys, types::*};
use std::ffi::CStr;

/// Device information structure
#[derive(Debug, Clone)]
pub struct DeviceInfo {
    /// Device name
    pub name: String,
    /// Supported pixel formats
    pub supported_pixel_formats: Vec<PixelFormat>,
    /// Supported resolutions
    pub supported_resolutions: Vec<Resolution>,
}

impl DeviceInfo {
    /// Create DeviceInfo from C structure
    pub fn from_c_struct(info: &sys::CcapDeviceInfo) -> Result<Self, CcapError> {
        let name_cstr = unsafe { CStr::from_ptr(info.deviceName.as_ptr()) };
        let name = name_cstr
            .to_str()
            .map_err(|e| CcapError::StringConversionError(e.to_string()))?
            .to_string();

        // Ensure we don't exceed array bounds
        let format_count = (info.pixelFormatCount).min(info.supportedPixelFormats.len());
        let supported_pixel_formats = info.supportedPixelFormats[..format_count]
            .iter()
            .map(|&format| PixelFormat::from_c_enum(format))
            .collect();

        let resolution_count = (info.resolutionCount).min(info.supportedResolutions.len());
        let supported_resolutions = info.supportedResolutions[..resolution_count]
            .iter()
            .map(|&res| Resolution::from(res))
            .collect();

        Ok(DeviceInfo {
            name,
            supported_pixel_formats,
            supported_resolutions,
        })
    }
}

/// Video frame wrapper
pub struct VideoFrame {
    frame: *mut sys::CcapVideoFrame,
    owns_frame: bool, // Whether we own the frame and should release it
}

impl VideoFrame {
    pub(crate) fn from_c_ptr(frame: *mut sys::CcapVideoFrame) -> Self {
        VideoFrame {
            frame,
            owns_frame: true,
        }
    }

    /// Create frame from raw pointer without owning it (for callbacks)
    pub(crate) fn from_c_ptr_ref(frame: *mut sys::CcapVideoFrame) -> Self {
        VideoFrame {
            frame,
            owns_frame: false,
        }
    }

    /// Get the internal C pointer (for internal use)
    #[allow(dead_code)]
    pub(crate) fn as_c_ptr(&self) -> *const sys::CcapVideoFrame {
        self.frame as *const sys::CcapVideoFrame
    }

    /// Create frame from raw pointer (for internal use)
    #[allow(dead_code)]
    pub(crate) fn from_raw(frame: *mut sys::CcapVideoFrame) -> Option<Self> {
        if frame.is_null() {
            None
        } else {
            Some(VideoFrame {
                frame,
                owns_frame: true,
            })
        }
    }

    /// Get frame information
    pub fn info<'a>(&'a self) -> crate::error::Result<VideoFrameInfo<'a>> {
        let mut info = sys::CcapVideoFrameInfo::default();

        let success = unsafe { sys::ccap_video_frame_get_info(self.frame, &mut info) };

        if success {
            // Calculate proper plane sizes based on pixel format
            // For plane 0 (Y or main): stride * height
            // For chroma planes (UV): stride * height/2 for most formats
            let plane0_size = (info.stride[0] as usize) * (info.height as usize);
            let plane1_size = if info.stride[1] > 0 {
                (info.stride[1] as usize) * ((info.height as usize + 1) / 2)
            } else {
                0
            };
            let plane2_size = if info.stride[2] > 0 {
                (info.stride[2] as usize) * ((info.height as usize + 1) / 2)
            } else {
                0
            };

            Ok(VideoFrameInfo {
                width: info.width,
                height: info.height,
                pixel_format: PixelFormat::from(info.pixelFormat),
                size_in_bytes: info.sizeInBytes,
                timestamp: info.timestamp,
                frame_index: info.frameIndex,
                orientation: FrameOrientation::from(info.orientation),
                data_planes: [
                    if info.data[0].is_null() {
                        None
                    } else {
                        Some(unsafe { std::slice::from_raw_parts(info.data[0], plane0_size) })
                    },
                    if info.data[1].is_null() {
                        None
                    } else {
                        Some(unsafe { std::slice::from_raw_parts(info.data[1], plane1_size) })
                    },
                    if info.data[2].is_null() {
                        None
                    } else {
                        Some(unsafe { std::slice::from_raw_parts(info.data[2], plane2_size) })
                    },
                ],
                strides: [info.stride[0], info.stride[1], info.stride[2]],
            })
        } else {
            Err(CcapError::FrameGrabFailed)
        }
    }

    /// Get all frame data as a slice
    pub fn data(&self) -> crate::error::Result<&[u8]> {
        let mut info = sys::CcapVideoFrameInfo::default();

        let success = unsafe { sys::ccap_video_frame_get_info(self.frame, &mut info) };

        if success && !info.data[0].is_null() {
            Ok(unsafe { std::slice::from_raw_parts(info.data[0], info.sizeInBytes as usize) })
        } else {
            Err(CcapError::FrameGrabFailed)
        }
    }

    /// Get frame width (convenience method)
    pub fn width(&self) -> u32 {
        self.info().map(|info| info.width).unwrap_or(0)
    }

    /// Get frame height (convenience method)
    pub fn height(&self) -> u32 {
        self.info().map(|info| info.height).unwrap_or(0)
    }

    /// Get pixel format (convenience method)
    pub fn pixel_format(&self) -> PixelFormat {
        self.info()
            .map(|info| info.pixel_format)
            .unwrap_or(PixelFormat::Unknown)
    }

    /// Get data size in bytes (convenience method)
    pub fn data_size(&self) -> u32 {
        self.info().map(|info| info.size_in_bytes).unwrap_or(0)
    }

    /// Get frame index (convenience method)
    pub fn index(&self) -> u64 {
        self.info().map(|info| info.frame_index).unwrap_or(0)
    }
}

impl Drop for VideoFrame {
    fn drop(&mut self) {
        if self.owns_frame {
            unsafe {
                sys::ccap_video_frame_release(self.frame);
            }
        }
    }
}

// # Thread Safety Analysis for VideoFrame
//
// ## Send Implementation
//
// SAFETY: VideoFrame is Send because:
// 1. The frame data pointer is obtained via ccap_video_frame_grab() which returns
//    a new frame that is independent of the Provider
// 2. The frame memory is managed by the C library with proper reference counting
// 3. Once created, the frame data is immutable from the Rust side
// 4. The ccap_video_frame_release() function is safe to call from any thread
//
// ## Why NOT Sync
//
// VideoFrame does NOT implement Sync because:
// 1. The underlying C++ VideoFrame object may have internal mutable state
// 2. Concurrent read access from multiple threads is not verified safe
// 3. The C++ library does not document thread-safety guarantees for frame access
//
// ## Usage Guidelines
//
// - Safe: Moving a VideoFrame to another thread (Send)
// - Safe: Dropping a VideoFrame on any thread
// - NOT Safe: Sharing &VideoFrame between threads (no Sync)
// - For multi-threaded access: Clone the frame data to owned Vec<u8> first
//
// ## Verification Status
//
// This thread-safety analysis is based on code inspection of the C++ implementation.
// The ccap C++ library does not provide formal thread-safety documentation.
// If you encounter issues with cross-thread frame usage, please report them at:
// https://github.com/wysaid/CameraCapture/issues
unsafe impl Send for VideoFrame {}

/// High-level video frame information
#[derive(Debug)]
pub struct VideoFrameInfo<'a> {
    /// Frame width in pixels
    pub width: u32,
    /// Frame height in pixels
    pub height: u32,
    /// Pixel format of the frame
    pub pixel_format: PixelFormat,
    /// Size of frame data in bytes
    pub size_in_bytes: u32,
    /// Frame timestamp
    pub timestamp: u64,
    /// Frame sequence index
    pub frame_index: u64,
    /// Frame orientation
    pub orientation: FrameOrientation,
    /// Frame data planes (up to 3 planes)
    pub data_planes: [Option<&'a [u8]>; 3],
    /// Stride values for each plane
    pub strides: [u32; 3],
}
