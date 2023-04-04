// Since we're running on bare metal, we disable the standard library.
#![no_std]
// To implement our formatting functions we want to directly interface with the Rust formatting runtime.
// This feature gives us access to the "unstable" interface.
#![feature(fmt_internals)]
// To implement our panic handler so that it can actually display the majority of panics, we need access to the message.
#![feature(panic_info_message)]

// We use a couple modules for code organization.
// Each module declaration causes the compiler to search for `src/<name>.rs` or `src/<name>/mod.rs`.
// We have used both variants, so we have `src/api/mod.rs` as well as `src/writer.rs`.
// (Generally, we use the `mod.rs` variant when the module has submodules and thus its own directory.)
// A module introduces a new scope, unlike files in C.

mod api;
mod cstr;
mod panic;
mod writer;
