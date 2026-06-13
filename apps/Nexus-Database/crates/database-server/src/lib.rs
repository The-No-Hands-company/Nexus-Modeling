//! Nexus Database Server — library target for testing and embedding.
//!
//! Re-exports all modules so the binary and integration tests share
//! the same code. The binary lives in `main.rs`.

pub mod auth;
pub mod cloud;
pub mod persistence;
pub mod routes;
pub mod state;
