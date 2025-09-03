use ccap::{Provider, Result, LogLevel, Utils};

fn find_camera_names() -> Result<Vec<String>> {
    // Create a temporary provider to query devices
    let provider = Provider::new()?;
    let devices = provider.list_devices()?;
    
    if !devices.is_empty() {
        println!("## Found {} video capture device:", devices.len());
        for (index, name) in devices.iter().enumerate() {
            println!("    {}: {}", index, name);
        }
    } else {
        eprintln!("Failed to find any video capture device.");
    }
    
    Ok(devices)
}

fn print_camera_info(device_name: &str) -> Result<()> {
    Utils::set_log_level(LogLevel::Verbose);
    
    // Create provider with specific device name
    let provider = match Provider::with_device_name(device_name) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("### Failed to create provider for device: {}, error: {}", device_name, e);
            return Ok(());
        }
    };
    
    match provider.device_info() {
        Ok(device_info) => {
            println!("===== Info for device: {} =======", device_name);
            
            println!("  Supported resolutions:");
            for resolution in &device_info.supported_resolutions {
                println!("    {}x{}", resolution.width, resolution.height);
            }
            
            println!("  Supported pixel formats:");
            for format in &device_info.supported_pixel_formats {
                println!("    {}", format.as_str());
            }
            
            println!("===== Info end =======\n");
        }
        Err(e) => {
            eprintln!("Failed to get device info for: {}, error: {}", device_name, e);
        }
    }
    
    Ok(())
}

fn main() -> Result<()> {
    // Set error callback to receive error notifications
    Provider::set_error_callback(|error_code, description| {
        eprintln!("Camera Error - Code: {}, Description: {}", error_code, description);
    });
    
    let device_names = find_camera_names()?;
    if device_names.is_empty() {
        return Ok(());
    }
    
    for name in &device_names {
        if let Err(e) = print_camera_info(name) {
            eprintln!("Error processing device {}: {}", name, e);
        }
    }
    
    Ok(())
}
