//! # ccap - Cross-platform Camera Capture Library
//!
//! A high-performance, lightweight camera capture library with Rust bindings.

#![warn(missing_docs)]
#![warn(rust_2018_idioms)]

// Re-export the low-level bindings for advanced users
pub mod sys {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod error;

// Public re-exports
pub use error::{CcapError, Result};
