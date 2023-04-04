use core::ffi::c_char;
use core::panic::PanicInfo;

use crate::cstr::{cstr, SmallCString};

// This is how we declare a C function so we can call it from Rust.
extern "C" {
	// `-> !` means that the function never returns.
	fn halt() -> !;
	// No return type is the unit type `()`, which is equivalent to `void` in C.
	//
	// While Rust does not support variadic arguments normally, it can call C variadic functions.
	// To use this feature, we write `...` just like in C.
	// Of course, we have to be careful to get the types right, and for this reason it is certainly `unsafe` (implied by being in an `extern "C"` block).
	fn log_write(file: *const c_char, line: u32, level: u32, fmt: *const c_char, ...);
}

// Rust uses dynamic typing via the `Any` trait for its panic payloads.
// Normally the payload can be `&str` or `String`, but since we are in a `#![no_std]` environment, it can only be `&str`.
// However, again since we are in a `#![no_std]` environment, we need to consider format arguments that may have been directly attached.
// (In `std` environments, the format runtime will simply allocate a `String` and format into that, then attach it via the normal payload.)
fn extract_message(info: &PanicInfo) -> SmallCString<256> {
	// This "method chain" is an example of Rust's functional programming capabilities.
	info.message().map_or_else(
		|| {
			info
				.payload()
				.downcast_ref::<&str>()
				.copied()
				.unwrap_or("Box<dyn Any>")
				// Using the `From` implementation for `SmallCString`, we can concisely convert from the `&str` payload.
				.into()
		},
		|format_args| SmallCString::from_format(*format_args),
	)
}

// Make sure to keep this in sync with `include/log.h`!
const LEVEL_FATAL: u32 = 60;

// Rust calls this function if any of our code panics.
// Since we have nice logging utilities, we use those, and then halt to avoid burning up the CPU with an infinite loop.
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
	let (file, line) = info.location().map_or(("<unknown>", 0), |location| {
		(location.file(), location.line())
	});
	// Note that we assign these C strings to bindings rather than doing `cstr(...).as_c()` in the `log_write` call.
	// This is to make sure that the stack-allocated storage stays alive during the `log_write` call.
	let message = extract_message(info);
	let file = cstr::<128>(file);
	let c_format = cstr::<32>(b"panicked: %s");
	unsafe {
		log_write(
			file.as_c(),
			line,
			LEVEL_FATAL,
			c_format.as_c(),
			message.as_c(),
		);
		halt();
	}
}
