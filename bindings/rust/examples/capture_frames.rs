//! Frame capture example
//! 
//! This example demonstrates how to capture frames from a camera device.

use ccap::{Provider, Result};
use std::time::{Duration, Instant};

fn main() -> Result<()> {
    println!("ccap Rust Bindings - Frame Capture Example");
    println!("==========================================");

    // Create provider and find cameras
    let mut provider = Provider::new()?;
    let devices = provider.find_device_names()?;

    if devices.is_empty() {
        println!("No camera devices found.");
        return Ok(());
    }

    println!("Using camera: {}", devices[0]);

    // Open the first camera
    provider.open(Some(&devices[0]), true)?;
    println!("Camera opened and started successfully");

    // Get device info
    if let Ok(info) = provider.device_info() {
        println!("Camera: {}", info.name);
        println!("Supported formats: {:?}", info.supported_pixel_formats);
    }

    // Set error callback
    provider.set_error_callback(|error, description| {
        eprintln!("Camera error: {:?} - {}", error, description);
    });

    println!();
    println!("Capturing frames (press Ctrl+C to stop)...");

    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut last_report = Instant::now();

    loop {
        match provider.grab_frame(1000) { // 1 second timeout
            Ok(Some(frame)) => {
                frame_count += 1;
                
                // Get frame information
                if let Ok(info) = frame.info() {
                    // Report every 30 frames or 5 seconds
                    let now = Instant::now();
                    if frame_count % 30 == 0 || now.duration_since(last_report) > Duration::from_secs(5) {
                        let elapsed = now.duration_since(start_time);
                        let fps = frame_count as f64 / elapsed.as_secs_f64();
                        
                        println!("Frame {}: {}x{} {:?} (Frame #{}, {:.1} FPS)", 
                                frame_count, 
                                info.width, 
                                info.height, 
                                info.pixel_format,
                                info.frame_index,
                                fps);
                        
                        last_report = now;
                    }
                } else {
                    println!("Frame {}: Failed to get frame info", frame_count);
                }
            }
            Ok(None) => {
                println!("No frame available (timeout)");
            }
            Err(e) => {
                eprintln!("Frame capture error: {:?}", e);
                break;
            }
        }

        // Stop after 300 frames for demo purposes
        if frame_count >= 300 {
            break;
        }
    }

    println!();
    println!("Captured {} frames", frame_count);
    println!("Total time: {:.2}s", start_time.elapsed().as_secs_f64());

    provider.stop();
    println!("Camera stopped");

    Ok(())
}
