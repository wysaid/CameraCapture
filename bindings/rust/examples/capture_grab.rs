use ccap::{Provider, Convert, Utils, Result, PixelFormat};
use std::thread;
use std::time::Duration;

fn main() -> Result<()> {
    // Create a camera provider
    let mut provider = Provider::new()?;
    
    // List devices
    let devices = provider.list_devices()?;
    if devices.is_empty() {
        eprintln!("No camera devices found.");
        return Ok(());
    }
    
    println!("Found {} camera device(s):", devices.len());
    for (i, device) in devices.iter().enumerate() {
        println!("  {}: {}", i, device);
    }
    
    // Open the first device
    provider.open_device(Some(&devices[0]), true)?;
    println!("Opened device: {}", devices[0]);
    
    // Get device info
    let device_info = provider.device_info()?;
    println!("Device info:");
    println!("  Supported pixel formats: {:?}", device_info.supported_pixel_formats);
    println!("  Supported resolutions: {:?}", device_info.supported_resolutions);
    
    // Try to set a common resolution
    if let Some(res) = device_info.supported_resolutions.first() {
        provider.set_resolution(res.width, res.height)?;
        println!("Set resolution to {}x{}", res.width, res.height);
    }
    
    // Try to set RGB24 format if supported
    if device_info.supported_pixel_formats.contains(&PixelFormat::Rgb24) {
        provider.set_pixel_format(PixelFormat::Rgb24)?;
        println!("Set pixel format to RGB24");
    }
    
    // Print current settings
    let resolution = provider.resolution();
    let pixel_format = provider.pixel_format();
    let frame_rate = provider.frame_rate();
    
    println!("Current settings:");
    println!("  Resolution: {}x{}", resolution.width, resolution.height);
    println!("  Pixel format: {:?}", pixel_format);
    println!("  Frame rate: {:.2} fps", frame_rate);
    
    // Capture and save a frame
    println!("Grabbing a frame...");
    match provider.grab_frame(2000) {
        Ok(Some(frame)) => {
            println!("Captured frame: {}x{}, format: {:?}, size: {} bytes",
                frame.width(), frame.height(), frame.pixel_format(), frame.data_size());
            
            // Save the frame as BMP
            match Utils::save_frame_as_bmp(&frame, "captured_frame.bmp") {
                Ok(()) => println!("Frame saved as 'captured_frame.bmp'"),
                Err(e) => eprintln!("Failed to save frame: {}", e),
            }
            
            // If it's not RGB24, try to convert it
            if frame.pixel_format() != PixelFormat::Rgb24 {
                match Convert::convert_frame(&frame, PixelFormat::Rgb24) {
                    Ok(rgb_frame) => {
                        println!("Converted frame to RGB24");
                        match Utils::save_frame_as_bmp(&rgb_frame, "converted_frame.bmp") {
                            Ok(()) => println!("Converted frame saved as 'converted_frame.bmp'"),
                            Err(e) => eprintln!("Failed to save converted frame: {}", e),
                        }
                    }
                    Err(e) => eprintln!("Failed to convert frame: {}", e),
                }
            }
        }
        Ok(None) => {
            println!("No frame available (timeout)");
        }
        Err(e) => {
            eprintln!("Error grabbing frame: {}", e);
        }
    }
    
    Ok(())
}
