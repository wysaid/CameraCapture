//! Integration tests for ccap rust bindings
//!
//! Tests the main API functionality

use ccap::{CcapError, PixelFormat, Provider, Result};

fn skip_camera_tests() -> bool {
    std::env::var("CCAP_SKIP_CAMERA_TESTS").is_ok()
}

#[test]
fn test_provider_creation() -> Result<()> {
    let provider = Provider::new()?;
    assert!(!provider.is_opened());
    Ok(())
}

#[test]
fn test_library_version() -> Result<()> {
    let version = ccap::version()?;
    assert!(!version.is_empty());
    assert!(version.contains('.'));
    println!("ccap version: {}", version);
    Ok(())
}

#[test]
fn test_device_listing() -> Result<()> {
    if skip_camera_tests() {
        eprintln!("Skipping device_listing due to CCAP_SKIP_CAMERA_TESTS");
        return Ok(());
    }
    let provider = Provider::new()?;
    let devices = provider.list_devices()?;
    // In test environment we might not have cameras, so just check it doesn't crash
    println!("Found {} devices", devices.len());
    for (i, device) in devices.iter().enumerate() {
        println!("Device {}: {}", i, device);
    }
    Ok(())
}

#[test]
fn test_pixel_format_conversion() {
    let format = PixelFormat::Rgb24;
    let c_format = format.to_c_enum();
    let format_back = PixelFormat::from_c_enum(c_format);
    assert_eq!(format, format_back);
}

#[test]
fn test_error_types() {
    let error = CcapError::NoDeviceFound;
    let error_str = format!("{}", error);
    assert!(error_str.contains("No camera device found"));
}

#[test]
fn test_provider_with_index() {
    if skip_camera_tests() {
        eprintln!("Skipping provider_with_index due to CCAP_SKIP_CAMERA_TESTS");
        return;
    }
    // This might fail if no device at index 0, but should not crash
    match Provider::with_device(0) {
        Ok(_provider) => {
            println!("Successfully created provider with device 0");
        }
        Err(e) => {
            println!("Expected error for device 0: {}", e);
        }
    }
}

#[test]
fn test_device_operations_without_camera() {
    if skip_camera_tests() {
        eprintln!("Skipping device_operations_without_camera due to CCAP_SKIP_CAMERA_TESTS");
        return;
    }
    // Test that operations work regardless of camera presence
    let provider = Provider::new().expect("Failed to create provider");

    // These should work with or without cameras
    let devices = provider.list_devices().expect("Failed to list devices");
    println!("Found {} device(s)", devices.len());

    let version = Provider::version().expect("Failed to get version");
    assert!(!version.is_empty());
}
