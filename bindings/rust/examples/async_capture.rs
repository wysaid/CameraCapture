//! Async frame capture example
//! 
//! This example demonstrates asynchronous frame capture using tokio.

#[cfg(feature = "async")]
use ccap::r#async::AsyncProvider;
use ccap::Result;

#[cfg(feature = "async")]
#[tokio::main]
async fn main() -> Result<()> {
    println!("ccap Rust Bindings - Async Frame Capture Example");
    println!("================================================");

    // Create async provider
    let provider = AsyncProvider::new().await?;
    
    // Find cameras
    let devices = provider.find_device_names().await?;
    
    if devices.is_empty() {
        println!("No camera devices found.");
        return Ok(());
    }

    println!("Using camera: {}", devices[0]);

    // Open and start the camera
    provider.open(Some(&devices[0]), true).await?;
    println!("Camera opened and started successfully");

    println!("Capturing frames asynchronously...");

    let mut frame_count = 0;
    let start_time = std::time::Instant::now();

    loop {
        match provider.grab_frame().await {
            Ok(Some(frame)) => {
                frame_count += 1;
                
                if let Ok(info) = frame.info() {
                    if frame_count % 30 == 0 {
                        let elapsed = start_time.elapsed();
                        let fps = frame_count as f64 / elapsed.as_secs_f64();
                        
                        println!("Frame {}: {}x{} {:?} ({:.1} FPS)", 
                                frame_count, 
                                info.width, 
                                info.height, 
                                info.pixel_format,
                                fps);
                    }
                }
            }
            Ok(None) => {
                // No frame available, yield control
                tokio::task::yield_now().await;
            }
            Err(e) => {
                eprintln!("Frame capture error: {:?}", e);
                break;
            }
        }

        // Stop after 300 frames
        if frame_count >= 300 {
            break;
        }
    }

    provider.stop().await;
    println!("Async capture completed. Total frames: {}", frame_count);

    Ok(())
}

#[cfg(not(feature = "async"))]
fn main() {
    println!("This example requires the 'async' feature to be enabled.");
    println!("Run with: cargo run --features async --example async_capture");
}
