//! Test ccap library exports

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
