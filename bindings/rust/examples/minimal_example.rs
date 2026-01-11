use ccap::{CcapError, Provider, Result, Utils};

fn main() -> Result<()> {
    // Set error callback to receive error notifications
    Provider::set_error_callback(|error_code, description| {
        eprintln!(
            "Error occurred - Code: {}, Description: {}",
            error_code, description
        );
    });

    let temp_provider = Provider::new()?;
    let devices = temp_provider.list_devices()?;
    let camera_index = Utils::select_camera(&devices)?;

    // Use device index instead of name to avoid issues
    let camera_index_i32 = i32::try_from(camera_index).map_err(|_| {
        CcapError::InvalidParameter(format!(
            "camera index {} does not fit into i32",
            camera_index
        ))
    })?;
    let mut provider = Provider::with_device(camera_index_i32)?;
    provider.open()?;
    provider.start()?;

    if !provider.is_started() {
        eprintln!("Failed to start camera!");
        return Ok(());
    }

    println!("Camera started successfully.");

    // Capture frames
    for i in 0..10 {
        match provider.grab_frame(3000) {
            Ok(Some(frame)) => {
                println!(
                    "VideoFrame {} grabbed: width = {}, height = {}, bytes: {}, format: {:?}",
                    frame.index(),
                    frame.width(),
                    frame.height(),
                    frame.data_size(),
                    frame.pixel_format()
                );
            }
            Ok(None) => {
                eprintln!("Failed to grab frame {}!", i);
                return Ok(());
            }
            Err(e) => {
                eprintln!("Error grabbing frame {}: {}", i, e);
                return Ok(());
            }
        }
    }

    println!("Captured 10 frames, stopping...");
    let _ = provider.stop();
    Ok(())
}
