#![no_std]

use core::panic::PanicInfo;

// We use a macro to declare multiple variants of this function based on the integer type.
// It takes `$name` as the name of the function and `$ty` as the integer type to be converted to a string.
macro_rules! make_int_fn {
	($name:ident, $ty:ty) => {
		// The `#[no_mangle]` attribute ensures that we will be able to call the function by name in our C code.
		// Rust will mangle the name otherwise.
		// We mark the function `unsafe` because if its arguments are not correct it could cause undefined behavior.
		// See the "safety comment" for details.
		// We mark the function `extern "C"` so that it uses the C calling convention and we can call it from C.
		/// # Safety
		///
		/// `buf` must point to an array large enough to store the formatted integer, plus one for the null terminator.
		/// 21 bytes is sufficient for `u64`, and 22 for `i64`.
		#[no_mangle]
		pub unsafe extern "C" fn $name(buf: *mut u8, value: $ty) {
			// The `itoa` crate uses their own `Buffer` type, so we create that here.
			let mut itoa_buf = itoa::Buffer::new();
			let string = itoa_buf.format(value);
			// Get the generated string out of the internal buffer and into the output buffer.
			core::ptr::copy_nonoverlapping(string.as_ptr(), buf, string.len());
			// Make sure we add a null terminator.
			*buf.add(string.len()) = b'\0';
		}
	};
}

// We generate two instances of the integer function, one for unsigned numbers and one for signed numbers.
make_int_fn!(u64_to_str, u64);
make_int_fn!(i64_to_str, i64);

// Unlike `itoa`, `ryu`, its floating-point companion, offers a "raw" API that makes our code simpler.
// It provides separate functions for `f32` (equivalent to `float` in C) and `f64`.
// In our case, it is simpler and easier to only support `f64` and promote `f32`s to `f64` as necessary.
/// # Safety
///
/// `buf` must point to an array large enough to store the formatted float, plus one for the null terminator.
/// 25 bytes is sufficient for `f64`.
#[no_mangle]
pub unsafe extern "C" fn f64_to_str(buf: *mut u8, value: f64) {
	let len = ryu::raw::format64(value, buf);
	*buf.add(len) = b'\0';
}

// This is how we declare a C function so we can call it from Rust.
extern "C" {
	// `-> !` means that the function never returns.
	fn halt() -> !;
}

// Rust calls this function if any of the code panics.
// In our case we don't expect panics, so if one occurs we will simply halt.
// (Rust will accept any panic handler that doesn't return, so something like an infinite loop would have also worked, but halting is more "correct".)
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
	unsafe {
		halt();
	}
}
