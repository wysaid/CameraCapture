use crate::{sys, error::CcapError, types::*};
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

        let supported_pixel_formats = info.supportedPixelFormats[..info.pixelFormatCount]
            .iter()
            .map(|&format| PixelFormat::from_c_enum(format))
            .collect();

        let supported_resolutions = info.supportedResolutions[..info.resolutionCount]
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
        VideoFrame { frame, owns_frame: true }
    }

    /// Create frame from raw pointer without owning it (for callbacks)
    pub(crate) fn from_c_ptr_ref(frame: *mut sys::CcapVideoFrame) -> Self {
        VideoFrame { frame, owns_frame: false }
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
            Some(VideoFrame { frame, owns_frame: true })
        }
    }

    /// Get frame information
    pub fn info<'a>(&'a self) -> crate::error::Result<VideoFrameInfo<'a>> {
        let mut info = sys::CcapVideoFrameInfo::default();
        
        let success = unsafe { sys::ccap_video_frame_get_info(self.frame, &mut info) };
        
        if success {
            Ok(VideoFrameInfo {
                width: info.width,
                height: info.height,
                pixel_format: PixelFormat::from(info.pixelFormat),
                size_in_bytes: info.sizeInBytes,
                timestamp: info.timestamp,
                frame_index: info.frameIndex,
                orientation: FrameOrientation::from(info.orientation),
                data_planes: [
                    if info.data[0].is_null() { None } else { Some(unsafe { 
                        std::slice::from_raw_parts(info.data[0], info.stride[0] as usize) 
                    }) },
                    if info.data[1].is_null() { None } else { Some(unsafe { 
                        std::slice::from_raw_parts(info.data[1], info.stride[1] as usize) 
                    }) },
                    if info.data[2].is_null() { None } else { Some(unsafe { 
                        std::slice::from_raw_parts(info.data[2], info.stride[2] as usize) 
                    }) },
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
            Ok(unsafe {
                std::slice::from_raw_parts(info.data[0], info.sizeInBytes as usize)
            })
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
        self.info().map(|info| info.pixel_format).unwrap_or(PixelFormat::Unknown)
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

// Make VideoFrame Send + Sync if the underlying C library supports it
unsafe impl Send for VideoFrame {}
unsafe impl Sync for VideoFrame {}

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
