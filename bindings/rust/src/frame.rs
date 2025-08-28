use crate::{sys, error::CcapError, types::*};
use std::ffi::{CStr, CString};
use std::ptr;

/// Device information structure
#[derive(Debug, Clone)]
pub struct DeviceInfo {
    pub name: String,
    pub supported_pixel_formats: Vec<PixelFormat>,
    pub supported_resolutions: Vec<Resolution>,
}

impl DeviceInfo {
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
}

impl VideoFrame {
    pub(crate) fn from_c_ptr(frame: *mut sys::CcapVideoFrame) -> Self {
        VideoFrame { frame }
    }

    pub(crate) fn as_c_ptr(&self) -> *const sys::CcapVideoFrame {
        self.frame as *const sys::CcapVideoFrame
    }

    pub(crate) fn from_raw(frame: *mut sys::CcapVideoFrame) -> Option<Self> {
        if frame.is_null() {
            None
        } else {
            Some(VideoFrame { frame })
        }
    }

    /// Get frame information
    pub fn info(&self) -> crate::error::Result<VideoFrameInfo> {
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
}

impl Drop for VideoFrame {
    fn drop(&mut self) {
        unsafe {
            sys::ccap_video_frame_release(self.frame);
        }
    }
}

// Make VideoFrame Send + Sync if the underlying C library supports it
unsafe impl Send for VideoFrame {}
unsafe impl Sync for VideoFrame {}

/// High-level video frame information
#[derive(Debug)]
pub struct VideoFrameInfo {
    pub width: u32,
    pub height: u32,
    pub pixel_format: PixelFormat,
    pub size_in_bytes: u32,
    pub timestamp: u64,
    pub frame_index: u64,
    pub orientation: FrameOrientation,
    pub data_planes: [Option<&'static [u8]>; 3],
    pub strides: [u32; 3],
}
