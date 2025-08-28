//! Basic camera enumeration example
//! 
//! This example shows how to discover and list available camera devices.

use ccap::{Provider, Result};

fn main() -> Result<()> {
    println!("ccap Rust Bindings - Camera Discovery Example");
    println!("==============================================");
    
    // Get library version
    match ccap::version() {
        Ok(version) => println!("ccap version: {}", version),
        Err(e) => println!("Failed to get version: {:?}", e),
    }
    println!();

    // Create a camera provider
    let provider = Provider::new()?;
    println!("Camera provider created successfully");

    // Find available cameras
    println!("Discovering camera devices...");
    match provider.find_device_names() {
        Ok(devices) => {
            if devices.is_empty() {
                println!("No camera devices found.");
            } else {
                println!("Found {} camera device(s):", devices.len());
                for (index, device_name) in devices.iter().enumerate() {
                    println!("  [{}] {}", index, device_name);
                }
                
                // Try to get info for the first device
                if let Ok(mut provider_with_device) = Provider::with_device(&devices[0]) {
                    if let Ok(info) = provider_with_device.device_info() {
                        println!();
                        println!("Device Info for '{}':", info.name);
                        println!("  Supported Pixel Formats: {:?}", info.supported_pixel_formats);
                        println!("  Supported Resolutions:");
                        for res in &info.supported_resolutions {
                            println!("    {}x{}", res.width, res.height);
                        }
                    }
                }
            }
        }
        Err(e) => {
            println!("Failed to discover cameras: {:?}", e);
        }
    }

    Ok(())
}
