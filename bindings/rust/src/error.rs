//! Error handling for ccap library

use thiserror::Error;

/// Error types for ccap operations
#[derive(Debug, Error)]
pub enum CcapError {
    /// No error occurred
    #[error("No error")]
    None,

    /// No camera device found
    #[error("No camera device found")]
    NoDeviceFound,

    /// Invalid device specified
    #[error("Invalid device: {0}")]
    InvalidDevice(String),

    /// Camera device open failed
    #[error("Camera device open failed")]
    DeviceOpenFailed,

    /// Device already opened
    #[error("Device already opened")]
    DeviceAlreadyOpened,

    /// Device not opened
    #[error("Device not opened")]
    DeviceNotOpened,

    /// Capture start failed
    #[error("Capture start failed")]
    CaptureStartFailed,

    /// Capture stop failed
    #[error("Capture stop failed")]
    CaptureStopFailed,

    /// Frame grab failed
    #[error("Frame grab failed")]
    FrameGrabFailed,

    /// Timeout occurred
    #[error("Timeout occurred")]
    Timeout,

    /// Invalid parameter
    #[error("Invalid parameter: {0}")]
    InvalidParameter(String),

    /// Not supported operation
    #[error("Operation not supported")]
    NotSupported,

    /// Backend set failed
    #[error("Backend set failed")]
    BackendSetFailed,

    /// String conversion error
    #[error("String conversion error: {0}")]
    StringConversionError(String),

    /// File operation failed
    #[error("File operation failed: {0}")]
    FileOperationFailed(String),

    /// Device not found (alias for NoDeviceFound for compatibility)
    #[error("Device not found")]
    DeviceNotFound,

    /// Internal error
    #[error("Internal error: {0}")]
    InternalError(String),

    /// Unknown error with error code
    #[error("Unknown error: {code}")]
    Unknown {
        /// Error code from the underlying system
        code: i32,
    },
}

impl From<i32> for CcapError {
    fn from(code: i32) -> Self {
        use crate::sys::*;

        // Convert i32 to CcapErrorCode for matching
        // On some platforms CcapErrorCode might be unsigned
        let code_u = code as CcapErrorCode;

        #[allow(non_upper_case_globals)]
        match code_u {
            CcapErrorCode_CCAP_ERROR_NONE => CcapError::None,
            CcapErrorCode_CCAP_ERROR_NO_DEVICE_FOUND => CcapError::NoDeviceFound,
            CcapErrorCode_CCAP_ERROR_INVALID_DEVICE => CcapError::InvalidDevice("".to_string()),
            CcapErrorCode_CCAP_ERROR_DEVICE_OPEN_FAILED => CcapError::DeviceOpenFailed,
            CcapErrorCode_CCAP_ERROR_DEVICE_START_FAILED => CcapError::CaptureStartFailed,
            CcapErrorCode_CCAP_ERROR_DEVICE_STOP_FAILED => CcapError::CaptureStopFailed,
            CcapErrorCode_CCAP_ERROR_FRAME_CAPTURE_FAILED => CcapError::FrameGrabFailed,
            CcapErrorCode_CCAP_ERROR_FRAME_CAPTURE_TIMEOUT => CcapError::Timeout,
            CcapErrorCode_CCAP_ERROR_UNSUPPORTED_PIXEL_FORMAT => CcapError::NotSupported,
            CcapErrorCode_CCAP_ERROR_UNSUPPORTED_RESOLUTION => CcapError::NotSupported,
            CcapErrorCode_CCAP_ERROR_PROPERTY_SET_FAILED => {
                CcapError::InvalidParameter("".to_string())
            }
            CcapErrorCode_CCAP_ERROR_MEMORY_ALLOCATION_FAILED => CcapError::Unknown { code },
            CcapErrorCode_CCAP_ERROR_INTERNAL_ERROR => CcapError::Unknown { code },
            _ => CcapError::Unknown { code },
        }
    }
}

/// Result type for ccap operations
pub type Result<T> = std::result::Result<T, CcapError>;
