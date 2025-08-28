use ccap::{Provider, Result};
use std::sync::{Arc, Mutex, mpsc};
use std::thread;
use std::time::{Duration, Instant};

fn main() -> Result<()> {
    // Create a camera provider
    let mut provider = Provider::new()?;
    
    // Open the first available device
    provider.open()?;
    println!("Camera opened successfully.");
    
    // Start capture
    provider.start()?;
    println!("Camera capture started.");
    
    // Statistics tracking
    let frame_count = Arc::new(Mutex::new(0u32));
    let start_time = Arc::new(Mutex::new(Instant::now()));
    
    // Create a channel for communication
    let (tx, rx) = mpsc::channel();
    
    // Spawn a thread to continuously grab frames
    let frame_count_clone = frame_count.clone();
    let start_time_clone = start_time.clone();
    
    thread::spawn(move || {
        loop {
            // Check for stop signal
            match rx.try_recv() {
                Ok(_) => break,
                Err(mpsc::TryRecvError::Disconnected) => break,
                Err(mpsc::TryRecvError::Empty) => {}
            }
            
            // Grab frame with timeout
            match provider.grab_frame(100) {
                Ok(Some(frame)) => {
                    let mut count = frame_count_clone.lock().unwrap();
                    *count += 1;
                    
                    // Print stats every 30 frames
                    if *count % 30 == 0 {
                        let elapsed = start_time_clone.lock().unwrap().elapsed();
                        let fps = *count as f64 / elapsed.as_secs_f64();
                        
                        println!("Frame {}: {}x{}, format: {:?}, FPS: {:.1}",
                            *count,
                            frame.width(),
                            frame.height(), 
                            frame.pixel_format(),
                            fps
                        );
                        
                        // TODO: Save every 30th frame (saving not yet implemented)
                        println!("Frame {} captured: {}x{}, format: {:?} (saving not implemented)", 
                                *count, frame.width(), frame.height(), frame.pixel_format());
                    }
                }
                Ok(None) => {
                    // No frame available, continue
                }
                Err(e) => {
                    eprintln!("Error grabbing frame: {}", e);
                    thread::sleep(Duration::from_millis(10));
                }
            }
        }
        
        println!("Frame grabbing thread stopped.");
    });
    
    // Run for 10 seconds
    println!("Capturing frames for 10 seconds...");
    thread::sleep(Duration::from_secs(10));
    
    // Signal stop
    let _ = tx.send(());
    
    // Wait a bit for thread to finish
    thread::sleep(Duration::from_millis(100));
    
    // Print final statistics
    let final_count = *frame_count.lock().unwrap();
    let total_time = start_time.lock().unwrap().elapsed();
    let avg_fps = final_count as f64 / total_time.as_secs_f64();
    
    println!("Capture completed:");
    println!("  Total frames: {}", final_count);
    println!("  Total time: {:.2}s", total_time.as_secs_f64());
    println!("  Average FPS: {:.1}", avg_fps);
    
    Ok(())
}
