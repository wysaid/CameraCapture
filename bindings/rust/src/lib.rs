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

mod error;
mod types;
mod frame;
mod provider;
// TODO: Fix these modules later
// mod convert;
// mod utils;

#[cfg(feature = "async")]
pub mod r#async;

// Public re-exports
pub use error::{CcapError, Result};
pub use types::*;
pub use frame::*;
pub use provider::Provider;
// pub use convert::Convert;
// pub use utils::Utils;

/// Get library version string
pub fn version() -> Result<String> {
    Provider::version()
}
