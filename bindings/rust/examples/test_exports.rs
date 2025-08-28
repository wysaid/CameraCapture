fn main() {
    // Test each export individually
    println!("Testing ccap::sys...");
    let _ = ccap::sys::CCAP_MAX_DEVICES;
    
    println!("Testing ccap::CcapError...");
    let _ = ccap::CcapError::None;
    
    println!("Testing ccap::Result...");
    let result: ccap::Result<i32> = Ok(42);
    println!("Result: {:?}", result);
    
    println!("All exports work!");
}