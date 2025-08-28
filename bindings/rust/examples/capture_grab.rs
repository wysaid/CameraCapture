use ccap::{Provider, Result, PixelFormat};

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
    let resolution = provider.resolution()?;
    let pixel_format = provider.pixel_format()?;
    let frame_rate = provider.frame_rate()?;
    
    println!("Current settings:");
    println!("  Resolution: {}x{}", resolution.0, resolution.1);
    println!("  Pixel format: {:?}", pixel_format);
    println!("  Frame rate: {:.2} fps", frame_rate);
    
    // Capture and save a frame
    println!("Grabbing a frame...");
    match provider.grab_frame(2000) {
        Ok(Some(frame)) => {
            println!("Captured frame: {}x{}, format: {:?}, size: {} bytes",
                frame.width(), frame.height(), frame.pixel_format(), frame.data_size());
            
            // TODO: Add frame saving functionality
            println!("Frame captured successfully (saving not yet implemented)");
            
            // TODO: Add frame conversion functionality
            if frame.pixel_format() != PixelFormat::Rgb24 {
                println!("Frame format conversion not yet implemented");
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
