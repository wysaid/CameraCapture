use crate::error::CcapError;
use crate::types::PixelFormat;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sys;

    #[test]
    fn test_pixel_format_conversion() {
        let pf = PixelFormat::from(sys::CcapPixelFormat_CCAP_PIXEL_FORMAT_NV12);
        assert_eq!(pf, PixelFormat::Nv12);
    }

    #[test]
    fn test_error_conversion() {
        let error = CcapError::from(sys::CcapErrorCode_CCAP_ERROR_NO_DEVICE_FOUND);
        match error {
            CcapError::NoDeviceFound => {},
            _ => panic!("Unexpected error type")
        }
    }

    #[test]
    fn test_constants() {
        assert!(sys::CCAP_MAX_DEVICES > 0);
        assert!(sys::CCAP_MAX_DEVICE_NAME_LENGTH > 0);
    }
}
