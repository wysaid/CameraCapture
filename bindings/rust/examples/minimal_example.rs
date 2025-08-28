use ccap::{Provider, Result};
use std::thread;
use std::time::Duration;

fn main() -> Result<()> {
    // Create a camera provider and open the first device
    let mut provider = Provider::new()?;
    
    // Open device with auto-start
    provider.open()?;
    println!("Camera opened successfully.");
    
    // Check if capture is started
    if provider.is_started() {
        println!("Camera capture started.");
    } else {
        println!("Starting camera capture...");
        provider.start()?;
    }
    
    // Capture a few frames
    println!("Capturing frames...");
    for i in 0..5 {
        match provider.grab_frame(1000) {
            Ok(Some(frame)) => {
                let width = frame.width();
                let height = frame.height();
                let format = frame.pixel_format();
                let data_size = frame.data_size();
                
                println!("Frame {}: {}x{}, format: {:?}, size: {} bytes", 
                    i + 1, width, height, format, data_size);
            }
            Ok(None) => {
                println!("Frame {}: No frame available (timeout)", i + 1);
            }
            Err(e) => {
                eprintln!("Frame {}: Error grabbing frame: {}", i + 1, e);
            }
        }
        
        // Small delay between captures
        thread::sleep(Duration::from_millis(100));
    }
    
    // Stop capture
    provider.stop();
    println!("Camera capture stopped.");
    
    Ok(())
}
