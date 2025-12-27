//! Camera provider for synchronous camera capture operations

use crate::{sys, error::*, types::*, frame::*};
use std::ffi::{CStr, CString};
use std::ptr;

/// Camera provider for synchronous camera capture operations
pub struct Provider {
    handle: *mut sys::CcapProvider,
    is_opened: bool,
    callback_ptr: Option<*mut std::ffi::c_void>,
}

unsafe impl Send for Provider {}

impl Provider {
    /// Create a new camera provider
    pub fn new() -> Result<Self> {
        let handle = unsafe { sys::ccap_provider_create() };
        if handle.is_null() {
            return Err(CcapError::DeviceOpenFailed);
        }
        
        Ok(Provider {
            handle,
            is_opened: false,
            callback_ptr: None,
        })
    }
    
    /// Create a provider with a specific device index
    pub fn with_device(device_index: i32) -> Result<Self> {
        let handle = unsafe { sys::ccap_provider_create_with_index(device_index, ptr::null()) };
        if handle.is_null() {
            return Err(CcapError::InvalidDevice(format!("device index {}", device_index)));
        }
        
        Ok(Provider {
            handle,
            is_opened: true, // C API likely opens device automatically
            callback_ptr: None,
        })
    }
    
    /// Create a provider with a specific device name
    pub fn with_device_name<S: AsRef<str>>(device_name: S) -> Result<Self> {
        let c_name = CString::new(device_name.as_ref())
            .map_err(|_| CcapError::InvalidParameter("device name contains null byte".to_string()))?;
            
        let handle = unsafe { sys::ccap_provider_create_with_device(c_name.as_ptr(), ptr::null()) };
        if handle.is_null() {
            return Err(CcapError::InvalidDevice(device_name.as_ref().to_string()));
        }
        
        Ok(Provider {
            handle,
            is_opened: true, // C API likely opens device automatically
            callback_ptr: None,
        })
    }
    
    /// Get available camera devices
    pub fn get_devices() -> Result<Vec<DeviceInfo>> {
        // Create a temporary provider to query devices
        let provider = Self::new()?;
        let mut device_names_list = sys::CcapDeviceNamesList::default();
        
        let success = unsafe {
            sys::ccap_provider_find_device_names_list(provider.handle, &mut device_names_list)
        };
        
        if !success {
            return Ok(Vec::new());
        }
        
        let mut devices = Vec::new();
        for i in 0..device_names_list.deviceCount {
            let name_bytes = &device_names_list.deviceNames[i];
            let name = unsafe {
                let cstr = CStr::from_ptr(name_bytes.as_ptr());
                cstr.to_string_lossy().to_string()
            };
            
            // Try to get device info by creating provider with this device
            if let Ok(device_provider) = Self::with_device_name(&name) {
                if let Ok(device_info) = device_provider.get_device_info_direct() {
                    devices.push(device_info);
                } else {
                    // Fallback: create minimal device info from just the name
                    devices.push(DeviceInfo {
                        name,
                        supported_pixel_formats: Vec::new(),
                        supported_resolutions: Vec::new(),
                    });
                }
            }
        }
        
        Ok(devices)
    }
    
    /// Get device info directly from current provider
    fn get_device_info_direct(&self) -> Result<DeviceInfo> {
        let mut device_info = sys::CcapDeviceInfo::default();
        
        let success = unsafe {
            sys::ccap_provider_get_device_info(self.handle, &mut device_info)
        };
        
        if !success {
            return Err(CcapError::DeviceOpenFailed);
        }
        
        let name = unsafe {
            let cstr = CStr::from_ptr(device_info.deviceName.as_ptr());
            cstr.to_string_lossy().to_string()
        };
        
        let mut formats = Vec::new();
        for i in 0..device_info.pixelFormatCount {
            if i < device_info.supportedPixelFormats.len() {
                formats.push(PixelFormat::from(device_info.supportedPixelFormats[i]));
            }
        }
        
        let mut resolutions = Vec::new();
        for i in 0..device_info.resolutionCount {
            if i < device_info.supportedResolutions.len() {
                let res = &device_info.supportedResolutions[i];
                resolutions.push(Resolution {
                    width: res.width,
                    height: res.height,
                });
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
        
        let result = unsafe { sys::ccap_provider_open_by_index(self.handle, -1, false) };
        if !result {
            return Err(CcapError::DeviceOpenFailed);
        }
        
        self.is_opened = true;
        Ok(())
    }
    
    /// Open device with optional device name and auto start
    pub fn open_device(&mut self, device_name: Option<&str>, auto_start: bool) -> Result<()> {
        if let Some(name) = device_name {
            // Recreate provider with specific device
            if !self.handle.is_null() {
                unsafe { sys::ccap_provider_destroy(self.handle); }
            }
            let c_name = CString::new(name)
                .map_err(|_| CcapError::InvalidParameter("device name contains null byte".to_string()))?;
            self.handle = unsafe { sys::ccap_provider_create_with_device(c_name.as_ptr(), ptr::null()) };
            if self.handle.is_null() {
                return Err(CcapError::InvalidDevice(name.to_string()));
            }
            self.is_opened = true;
        } else {
            self.open()?;
        }
        if auto_start {
            self.start_capture()?;
        }
        Ok(())
    }
    
    /// Get device info for the current provider
    pub fn device_info(&self) -> Result<DeviceInfo> {
        self.get_device_info_direct()
    }
    
    /// Check if capture is started
    pub fn is_started(&self) -> bool {
        unsafe { sys::ccap_provider_is_started(self.handle) }
    }
    
    /// Start capture (alias for start_capture)
    pub fn start(&mut self) -> Result<()> {
        self.start_capture()
    }
    
    /// Stop capture (alias for stop_capture)  
    pub fn stop(&mut self) -> Result<()> {
        self.stop_capture()
    }
    
    /// Check if the camera is opened
    pub fn is_opened(&self) -> bool {
        self.is_opened
    }
    
    /// Set camera property
    pub fn set_property(&mut self, property: PropertyName, value: f64) -> Result<()> {
        let property_id: sys::CcapPropertyName = property.into();
        let success = unsafe {
            sys::ccap_provider_set_property(self.handle, property_id, value)
        };
        
        if !success {
            return Err(CcapError::InvalidParameter(format!("property {:?}", property)));
        }
        
        Ok(())
    }
    
    /// Get camera property
    pub fn get_property(&self, property: PropertyName) -> Result<f64> {
        let property_id: sys::CcapPropertyName = property.into();
        let value = unsafe {
            sys::ccap_provider_get_property(self.handle, property_id)
        };
        
        Ok(value)
    }
    
    /// Set camera resolution
    pub fn set_resolution(&mut self, width: u32, height: u32) -> Result<()> {
        self.set_property(PropertyName::Width, width as f64)?;
        self.set_property(PropertyName::Height, height as f64)?;
        Ok(())
    }
    
    /// Set camera frame rate
    pub fn set_frame_rate(&mut self, fps: f64) -> Result<()> {
        self.set_property(PropertyName::FrameRate, fps)
    }
    
    /// Set pixel format
    pub fn set_pixel_format(&mut self, format: PixelFormat) -> Result<()> {
        self.set_property(
            PropertyName::PixelFormatOutput,
            format.to_c_enum() as u32 as f64,
        )
    }
    
    /// Grab a single frame with timeout
    pub fn grab_frame(&mut self, timeout_ms: u32) -> Result<Option<VideoFrame>> {
        if !self.is_opened {
            return Err(CcapError::DeviceNotOpened);
        }
        
        let frame = unsafe { sys::ccap_provider_grab(self.handle, timeout_ms) };
        if frame.is_null() {
            return Ok(None);
        }
        
        Ok(Some(VideoFrame::from_c_ptr(frame)))
    }
    
    /// Start continuous capture
    pub fn start_capture(&mut self) -> Result<()> {
        if !self.is_opened {
            return Err(CcapError::DeviceNotOpened);
        }
        
        let result = unsafe { sys::ccap_provider_start(self.handle) };
        if !result {
            return Err(CcapError::CaptureStartFailed);
        }
        
        Ok(())
    }
    
    /// Stop continuous capture
    pub fn stop_capture(&mut self) -> Result<()> {
        unsafe { sys::ccap_provider_stop(self.handle) };
        Ok(())
    }
    
    /// Get library version
    pub fn version() -> Result<String> {
        let version_ptr = unsafe { sys::ccap_get_version() };
        if version_ptr.is_null() {
            return Err(CcapError::Unknown { code: -1 });
        }
        
        let version_cstr = unsafe { CStr::from_ptr(version_ptr) };
        version_cstr
            .to_str()
            .map(|s| s.to_string())
            .map_err(|_| CcapError::Unknown { code: -2 })
    }
    
    /// List device names (simple string list)
    pub fn list_devices(&self) -> Result<Vec<String>> {
        let device_infos = Self::get_devices()?;
        Ok(device_infos.into_iter().map(|info| info.name).collect())
    }
    
    /// Find device names (alias for list_devices)
    pub fn find_device_names(&self) -> Result<Vec<String>> {
        self.list_devices()
    }
    
    /// Get current resolution (convenience getter)
    pub fn resolution(&self) -> Result<(u32, u32)> {
        let width = self.get_property(PropertyName::Width)? as u32;
        let height = self.get_property(PropertyName::Height)? as u32;
        Ok((width, height))
    }
    
    /// Get current pixel format (convenience getter)
    pub fn pixel_format(&self) -> Result<PixelFormat> {
        let format_val = self.get_property(PropertyName::PixelFormatOutput)? as u32;
        Ok(PixelFormat::from_c_enum(format_val as sys::CcapPixelFormat))
    }
    
    /// Get current frame rate (convenience getter)
    pub fn frame_rate(&self) -> Result<f64> {
        self.get_property(PropertyName::FrameRate)
    }

    /// Set error callback for camera errors
    pub fn set_error_callback<F>(callback: F) 
    where 
        F: Fn(u32, &str) + Send + Sync + 'static,
    {
        use std::sync::{Arc, Mutex};
        use std::os::raw::c_char;
        
        let callback = Arc::new(Mutex::new(callback));
        
        unsafe extern "C" fn error_callback_wrapper(
            error_code: sys::CcapErrorCode,
            description: *const c_char,
            user_data: *mut std::ffi::c_void,
        ) {
            if user_data.is_null() || description.is_null() {
                return;
            }
            
            let callback = &*(user_data as *const Arc<Mutex<dyn Fn(u32, &str) + Send + Sync>>);
            let desc_cstr = std::ffi::CStr::from_ptr(description);
            if let Ok(desc_str) = desc_cstr.to_str() {
                if let Ok(cb) = callback.lock() {
                    cb(error_code, desc_str);
                }
            }
        }
        
        // Store the callback to prevent it from being dropped
        let callback_ptr = Box::into_raw(Box::new(callback));
        
        unsafe {
            sys::ccap_set_error_callback(
                Some(error_callback_wrapper),
                callback_ptr as *mut std::ffi::c_void,
            );
        }
        
        // Note: This leaks memory, but it's acceptable for a global callback
        // In a production system, you'd want to provide a way to unregister callbacks
    }

    /// Open device with index and auto start
    pub fn open_with_index(&mut self, device_index: i32, auto_start: bool) -> Result<()> {
        // Destroy old handle if exists
        if !self.handle.is_null() {
            unsafe {
                sys::ccap_provider_destroy(self.handle);
            }
        }
        
        // Clean up old callback if exists
        self.cleanup_callback();
        
        // Create a new provider with the specified device index
        self.handle = unsafe { 
            sys::ccap_provider_create_with_index(device_index, ptr::null()) 
        };
        
        if self.handle.is_null() {
            return Err(CcapError::InvalidDevice(format!("device index {}", device_index)));
        }

        self.is_opened = false;
        self.open()?;
        if auto_start {
            self.start_capture()?;
        }
        Ok(())
    }

    /// Set a callback for new frame notifications
    pub fn set_new_frame_callback<F>(&mut self, callback: F) -> Result<()>
    where
        F: Fn(&VideoFrame) -> bool + Send + Sync + 'static,
    {
        use std::os::raw::c_void;
        
        // Clean up old callback if exists
        self.cleanup_callback();
        
        unsafe extern "C" fn new_frame_callback_wrapper(
            frame: *const sys::CcapVideoFrame,
            user_data: *mut c_void,
        ) -> bool {
            if user_data.is_null() || frame.is_null() {
                return false;
            }
            
            let callback = &*(user_data as *const Box<dyn Fn(&VideoFrame) -> bool + Send + Sync>);
            
            // Create a temporary VideoFrame wrapper that doesn't own the frame
            let video_frame = VideoFrame::from_c_ptr_ref(frame as *mut sys::CcapVideoFrame);
            callback(&video_frame)
        }
        
        // Store the callback to prevent it from being dropped
        let callback_box = Box::new(callback);
        let callback_ptr = Box::into_raw(callback_box);
        
        let success = unsafe {
            sys::ccap_provider_set_new_frame_callback(
                self.handle,
                Some(new_frame_callback_wrapper),
                callback_ptr as *mut c_void,
            )
        };
        
        if success {
            self.callback_ptr = Some(callback_ptr as *mut c_void);
            Ok(())
        } else {
            // Clean up on failure
            unsafe {
                let _ = Box::from_raw(callback_ptr);
            }
            Err(CcapError::CaptureStartFailed)
        }
    }

    /// Remove frame callback
    pub fn remove_new_frame_callback(&mut self) -> Result<()> {
        let success = unsafe {
            sys::ccap_provider_set_new_frame_callback(
                self.handle,
                None,
                ptr::null_mut(),
            )
        };
        
        if success {
            self.cleanup_callback();
            Ok(())
        } else {
            Err(CcapError::CaptureStopFailed)
        }
    }
    
    /// Clean up callback pointer
    fn cleanup_callback(&mut self) {
        if let Some(callback_ptr) = self.callback_ptr.take() {
            unsafe {
                let _ = Box::from_raw(callback_ptr as *mut Box<dyn Fn(&VideoFrame) -> bool + Send + Sync>);
            }
        }
    }
}

impl Drop for Provider {
    fn drop(&mut self) {
        // Clean up callback first
        self.cleanup_callback();
        
        if !self.handle.is_null() {
            unsafe {
                sys::ccap_provider_destroy(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}
