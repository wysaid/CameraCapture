//! # ccap - Cross-platform Camera Capture Library
//!
//! A high-performance, lightweight camera capture library with Rust bindings.

#![warn(missing_docs)]
#![warn(rust_2018_idioms)]

// Re-export the low-level bindings for advanced users
/// Low-level FFI bindings to ccap C library
pub mod sys {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    #![allow(missing_docs)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod convert;
mod error;
mod frame;
mod provider;
mod types;
mod utils;

// Public re-exports
pub use convert::Convert;
pub use error::{CcapError, Result};
pub use frame::*;
pub use provider::Provider;
pub use types::*;
pub use utils::{LogLevel, Utils};

/// Get library version string
pub fn version() -> Result<String> {
    Provider::version()
}
