use ccap::{Provider, Result};

fn main() -> Result<()> {
    // Print library version
    println!("ccap version: {}", Provider::version()?);
    
    // Create a camera provider
    let provider = Provider::new()?;
    println!("Provider created successfully.");
    
    // List all available devices
    match provider.list_devices() {
        Ok(devices) => {
            if devices.is_empty() {
                println!("No camera devices found.");
            } else {
                println!("Found {} camera device(s):", devices.len());
                for (i, device) in devices.iter().enumerate() {
                    println!("  {}: {}", i, device);
                }
            }
        }
        Err(e) => {
            eprintln!("Failed to list devices: {}", e);
        }
    }
    
    Ok(())
}
