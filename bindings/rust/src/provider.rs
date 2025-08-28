//! Camera provider for synchronous camera capture operations

use crate::{sys, error::*, types::*, frame::*};
use std::ffi::{CStr, CString};
use std::ptr;

/// Camera provider for synchronous camera capture operations
pub struct Provider {
    handle: *mut sys::CcapProvider,
    is_opened: bool,
}

unsafe impl Send for Provider {}

impl Provider {
    /// Create a new camera provider
    pub fn new() -> Result<Self> {
        let handle = unsafe { sys::ccap_create_provider() };
        if handle.is_null() {
            return Err(CcapError::DeviceOpenFailed);
        }
        
        Ok(Provider {
            handle,
            is_opened: false,
        })
    }
    
    /// Create a provider with a specific device index
    pub fn with_device(device_index: i32) -> Result<Self> {
        let handle = unsafe { sys::ccap_create_provider_with_device(device_index) };
        if handle.is_null() {
            return Err(CcapError::InvalidDevice(format!("device index {}", device_index)));
        }
        
        Ok(Provider {
            handle,
            is_opened: false,
        })
    }
    
    /// Create a provider with a specific device name
    pub fn with_device_name<S: AsRef<str>>(device_name: S) -> Result<Self> {
        let c_name = CString::new(device_name.as_ref())
            .map_err(|_| CcapError::InvalidParameter("device name contains null byte".to_string()))?;
            
        let handle = unsafe { sys::ccap_create_provider_with_device_name(c_name.as_ptr()) };
        if handle.is_null() {
            return Err(CcapError::InvalidDevice(device_name.as_ref().to_string()));
        }
        
        Ok(Provider {
            handle,
            is_opened: false,
        })
    }
    
    /// Get available camera devices
    pub fn get_devices() -> Result<Vec<DeviceInfo>> {
        let mut devices = Vec::new();
        let mut device_count = 0u32;
        
        // Get device count
        let result = unsafe {
            sys::ccap_get_device_count(&mut device_count as *mut u32)
        };
        
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        // Get each device info
        for i in 0..device_count {
            if let Ok(device_info) = Self::get_device_info(i as i32) {
                devices.push(device_info);
            }
        }
        
        Ok(devices)
    }
    
    /// Get device information for a specific device index
    pub fn get_device_info(device_index: i32) -> Result<DeviceInfo> {
        let mut name_buffer = [0i8; sys::CCAP_MAX_DEVICE_NAME_LENGTH as usize];
        
        let result = unsafe {
            sys::ccap_get_device_name(device_index, name_buffer.as_mut_ptr(), name_buffer.len() as u32)
        };
        
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        let name = unsafe { 
            CStr::from_ptr(name_buffer.as_ptr()).to_string_lossy().to_string()
        };
        
        // Get supported pixel formats
        let mut formats = Vec::new();
        let mut format_count = 0u32;
        
        let result = unsafe {
            sys::ccap_get_supported_pixel_formats(
                device_index,
                ptr::null_mut(),
                &mut format_count as *mut u32
            )
        };
        
        if result == sys::CcapErrorCode_CCAP_ERROR_NONE && format_count > 0 {
            let mut format_buffer = vec![0u32; format_count as usize];
            let result = unsafe {
                sys::ccap_get_supported_pixel_formats(
                    device_index,
                    format_buffer.as_mut_ptr(),
                    &mut format_count as *mut u32
                )
            };
            
            if result == sys::CcapErrorCode_CCAP_ERROR_NONE {
                for &format in &format_buffer {
                    formats.push(PixelFormat::from(format));
                }
            }
        }
        
        // Get supported resolutions
        let mut resolutions = Vec::new();
        let mut resolution_count = 0u32;
        
        let result = unsafe {
            sys::ccap_get_supported_resolutions(
                device_index,
                ptr::null_mut(),
                &mut resolution_count as *mut u32
            )
        };
        
        if result == sys::CcapErrorCode_CCAP_ERROR_NONE && resolution_count > 0 {
            let mut resolution_buffer = vec![sys::CcapResolution { width: 0, height: 0 }; resolution_count as usize];
            let result = unsafe {
                sys::ccap_get_supported_resolutions(
                    device_index,
                    resolution_buffer.as_mut_ptr(),
                    &mut resolution_count as *mut u32
                )
            };
            
            if result == sys::CcapErrorCode_CCAP_ERROR_NONE {
                for res in &resolution_buffer {
                    resolutions.push(Resolution {
                        width: res.width,
                        height: res.height,
                    });
                }
            }
        }
        
        Ok(DeviceInfo {
            name,
            supported_pixel_formats: formats,
            supported_resolutions: resolutions,
        })
    }
    
    /// Open the camera device
    pub fn open(&mut self) -> Result<()> {
        if self.is_opened {
            return Ok(());
        }
        
        let result = unsafe { sys::ccap_provider_open(self.handle) };
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        self.is_opened = true;
        Ok(())
    }
    
    /// Check if the camera is opened
    pub fn is_opened(&self) -> bool {
        self.is_opened
    }
    
    /// Set camera property
    pub fn set_property(&mut self, property: PropertyName, value: f64) -> Result<()> {
        let result = unsafe {
            sys::ccap_provider_set_property(self.handle, property as u32, value)
        };
        
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        Ok(())
    }
    
    /// Get camera property
    pub fn get_property(&self, property: PropertyName) -> Result<f64> {
        let mut value = 0.0;
        let result = unsafe {
            sys::ccap_provider_get_property(self.handle, property as u32, &mut value)
        };
        
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        Ok(value)
    }
    
    /// Set camera resolution
    pub fn set_resolution(&mut self, width: u32, height: u32) -> Result<()> {
        let result = unsafe {
            sys::ccap_provider_set_resolution(self.handle, width, height)
        };
        
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        Ok(())
    }
    
    /// Set camera frame rate
    pub fn set_frame_rate(&mut self, fps: f64) -> Result<()> {
        self.set_property(PropertyName::FrameRate, fps)
    }
    
    /// Set pixel format
    pub fn set_pixel_format(&mut self, format: PixelFormat) -> Result<()> {
        self.set_property(PropertyName::PixelFormatOutput, format as u32 as f64)
    }
    
    /// Grab a single frame with timeout
    pub fn grab_frame(&mut self, timeout_ms: u32) -> Result<Option<VideoFrame>> {
        if !self.is_opened {
            return Err(CcapError::DeviceNotOpened);
        }
        
        let frame = unsafe { sys::ccap_provider_grab_frame(self.handle, timeout_ms) };
        if frame.is_null() {
            return Ok(None);
        }
        
        Ok(Some(VideoFrame::from_handle(frame)))
    }
    
    /// Start continuous capture
    pub fn start_capture(&mut self) -> Result<()> {
        if !self.is_opened {
            return Err(CcapError::DeviceNotOpened);
        }
        
        let result = unsafe { sys::ccap_provider_start_capture(self.handle) };
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        Ok(())
    }
    
    /// Stop continuous capture
    pub fn stop_capture(&mut self) -> Result<()> {
        let result = unsafe { sys::ccap_provider_stop_capture(self.handle) };
        if result != sys::CcapErrorCode_CCAP_ERROR_NONE {
            return Err(CcapError::from(result as i32));
        }
        
        Ok(())
    }
}

impl Drop for Provider {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                sys::ccap_destroy_provider(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}
