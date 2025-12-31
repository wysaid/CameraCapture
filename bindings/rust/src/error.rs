//! Error handling for ccap library

/// Error types for ccap operations
#[derive(Debug)]
pub enum CcapError {
    /// No error occurred
    None,

    /// No camera device found
    NoDeviceFound,

    /// Invalid device specified
    InvalidDevice(String),

    /// Camera device open failed
    DeviceOpenFailed,

    /// Device already opened
    DeviceAlreadyOpened,

    /// Device not opened
    DeviceNotOpened,

    /// Capture start failed
    CaptureStartFailed,

    /// Capture stop failed
    CaptureStopFailed,

    /// Frame grab failed
    FrameGrabFailed,

    /// Timeout occurred
    Timeout,

    /// Invalid parameter
    InvalidParameter(String),

    /// Not supported operation
    NotSupported,

    /// Backend set failed
    BackendSetFailed,

    /// String conversion error
    StringConversionError(String),

    /// File operation failed
    FileOperationFailed(String),

    /// Device not found (alias for NoDeviceFound for compatibility)
    DeviceNotFound,

    /// Internal error
    InternalError(String),

    /// Unknown error with error code
    Unknown {
        /// Error code from the underlying system
        code: i32,
    },
}

impl std::fmt::Display for CcapError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CcapError::None => write!(f, "No error"),
            CcapError::NoDeviceFound => write!(f, "No camera device found"),
            CcapError::InvalidDevice(name) => write!(f, "Invalid device: {}", name),
            CcapError::DeviceOpenFailed => write!(f, "Camera device open failed"),
            CcapError::DeviceAlreadyOpened => write!(f, "Device already opened"),
            CcapError::DeviceNotOpened => write!(f, "Device not opened"),
            CcapError::CaptureStartFailed => write!(f, "Capture start failed"),
            CcapError::CaptureStopFailed => write!(f, "Capture stop failed"),
            CcapError::FrameGrabFailed => write!(f, "Frame grab failed"),
            CcapError::Timeout => write!(f, "Timeout occurred"),
            CcapError::InvalidParameter(param) => write!(f, "Invalid parameter: {}", param),
            CcapError::NotSupported => write!(f, "Operation not supported"),
            CcapError::BackendSetFailed => write!(f, "Backend set failed"),
            CcapError::StringConversionError(msg) => write!(f, "String conversion error: {}", msg),
            CcapError::FileOperationFailed(msg) => write!(f, "File operation failed: {}", msg),
            CcapError::DeviceNotFound => write!(f, "Device not found"),
            CcapError::InternalError(msg) => write!(f, "Internal error: {}", msg),
            CcapError::Unknown { code } => write!(f, "Unknown error: {}", code),
        }
    }
}

impl std::error::Error for CcapError {}

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
