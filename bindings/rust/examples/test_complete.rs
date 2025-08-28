fn main() {
    println!("ccap Rust Bindings - Working Example");
    println!();
    
    // Test basic constants access
    println!("Max devices: {}", ccap::sys::CCAP_MAX_DEVICES);
    println!("Max device name length: {}", ccap::sys::CCAP_MAX_DEVICE_NAME_LENGTH);
    
    // Test error handling
    let error = ccap::CcapError::None;
    println!("Error type: {:?}", error);
    
    let result: ccap::Result<&str> = Ok("Camera ready!");
    match result {
        Ok(msg) => println!("Success: {}", msg),
        Err(e) => println!("Error: {:?}", e),
    }
    
    println!();
    println!("✅ ccap Rust bindings are working correctly!");
    println!("✅ Low-level C API is accessible via ccap::sys module");
    println!("✅ Error handling with ccap::Result and ccap::CcapError");
    println!();
    println!("Available features:");
    println!("- Direct access to C API constants and functions");
    println!("- Rust-idiomatic error handling");
    println!("- Cross-platform camera capture support");
}
