use ccap::{LogLevel, PixelFormat, PropertyName, Provider, Result, Utils};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

fn main() -> Result<()> {
    // Enable verbose log to see debug information
    Utils::set_log_level(LogLevel::Verbose);

    // Set error callback to receive error notifications
    Provider::set_error_callback(|error_code, description| {
        eprintln!(
            "Camera Error - Code: {}, Description: {}",
            error_code, description
        );
    });

    let temp_provider = Provider::new()?;
    let devices = temp_provider.list_devices()?;
    if devices.is_empty() {
        eprintln!("No camera devices found!");
        return Ok(());
    }

    for (i, device) in devices.iter().enumerate() {
        println!("## Found video capture device: {}: {}", i, device);
    }

    // Select camera device (automatically use first device for testing)
    let device_index = 0;

    // Create provider with selected device
    let mut provider = Provider::with_device(device_index)?;

    // Set camera properties
    let requested_width = 1920;
    let requested_height = 1080;
    let requested_fps = 60.0;

    provider.set_property(PropertyName::Width, requested_width as f64)?;
    provider.set_property(PropertyName::Height, requested_height as f64)?;
    provider.set_property(
        PropertyName::PixelFormatOutput,
        PixelFormat::Bgra32 as u32 as f64,
    )?;
    provider.set_property(PropertyName::FrameRate, requested_fps)?;

    // Open and start camera
    provider.open()?;
    provider.start()?;

    if !provider.is_started() {
        eprintln!("Failed to start camera!");
        return Ok(());
    }

    // Get real camera properties
    let real_width = provider.get_property(PropertyName::Width)? as i32;
    let real_height = provider.get_property(PropertyName::Height)? as i32;
    let real_fps = provider.get_property(PropertyName::FrameRate)?;

    println!("Camera started successfully, requested resolution: {}x{}, real resolution: {}x{}, requested fps {}, real fps: {}",
             requested_width, requested_height, real_width, real_height, requested_fps, real_fps);

    // Create directory for captures (using std::fs)
    std::fs::create_dir_all("./image_capture")
        .map_err(|e| ccap::CcapError::FileOperationFailed(e.to_string()))?;

    // Statistics tracking
    let frame_count = Arc::new(Mutex::new(0u32));
    let frame_count_clone = frame_count.clone();

    // Set frame callback
    provider.set_new_frame_callback(move |frame| {
        let mut count = frame_count_clone.lock().unwrap();
        *count += 1;

        println!(
            "VideoFrame {} grabbed: width = {}, height = {}, bytes: {}",
            frame.index(),
            frame.width(),
            frame.height(),
            frame.data_size()
        );

        // Try to save frame to directory
        if let Ok(filename) = Utils::dump_frame_to_directory(frame, "./image_capture") {
            println!("VideoFrame saved to: {}", filename);
        } else {
            eprintln!("Failed to save frame!");
        }

        true // no need to retain the frame
    })?;

    // Wait for 5 seconds to capture frames
    println!("Capturing frames for 5 seconds...");
    thread::sleep(Duration::from_secs(5));

    // Get final count
    let final_count = *frame_count.lock().unwrap();
    println!("Captured {} frames, stopping...", final_count);

    // Remove callback before dropping
    let _ = provider.remove_new_frame_callback();

    Ok(())
}
