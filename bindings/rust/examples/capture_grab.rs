use ccap::{Provider, Result, PixelFormat, Utils, PropertyName, LogLevel};
use std::fs;

fn main() -> Result<()> {
    // Enable verbose log to see debug information
    Utils::set_log_level(LogLevel::Verbose);

    // Set error callback to receive error notifications
    Provider::set_error_callback(|error_code, description| {
        eprintln!("Camera Error - Code: {}, Description: {}", error_code, description);
    });

    // Create a camera provider
    let mut provider = Provider::new()?;

    // Open default device
    provider.open()?;
    provider.start_capture()?;

    if !provider.is_started() {
        eprintln!("Failed to start camera!");
        return Ok(());
    }

    // Print the real resolution and fps after camera started
    let real_width = provider.get_property(PropertyName::Width)? as u32;
    let real_height = provider.get_property(PropertyName::Height)? as u32;
    let real_fps = provider.get_property(PropertyName::FrameRate)?;

    println!("Camera started successfully, real resolution: {}x{}, real fps: {}",
           real_width, real_height, real_fps);

    // Create capture directory
    let capture_dir = "./image_capture";
    if !std::path::Path::new(capture_dir).exists() {
        fs::create_dir_all(capture_dir)
            .map_err(|e| ccap::CcapError::InvalidParameter(format!("Failed to create directory: {}", e)))?;
    }

    // Capture frames (3000 ms timeout when grabbing frames)
    let mut frame_count = 0;
    while let Some(frame) = provider.grab_frame(3000)? {
        let frame_info = frame.info()?;
        println!("VideoFrame {} grabbed: width = {}, height = {}, bytes: {}", 
            frame_info.frame_index, frame_info.width, frame_info.height, frame_info.size_in_bytes);
        
        // Save frame to directory
        match Utils::dump_frame_to_directory(&frame, capture_dir) {
            Ok(dump_file) => {
                println!("VideoFrame saved to: {}", dump_file);
            }
            Err(e) => {
                eprintln!("Failed to save frame: {}", e);
            }
        }

        frame_count += 1;
        if frame_count >= 10 {
            println!("Captured 10 frames, stopping...");
            break;
        }
    }

    Ok(())
}
