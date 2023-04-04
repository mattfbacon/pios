use core::ffi::{c_char, c_void};

/// Adapts a callback from C into a `core::fmt::Write` implementer.
#[derive(Clone, Copy)]
pub struct UserWriter {
	callback: unsafe extern "C" fn(*mut c_void, c_char),
	user_data: *mut c_void,
}

impl UserWriter {
	/// # Safety
	///
	/// `callback` will only be called with `user_data` as the first argument.
	/// No special restrictions apply to the second argument.
	/// Within those restrictions it must be sound.
	#[must_use]
	pub unsafe fn new(
		callback: unsafe extern "C" fn(*mut c_void, c_char),
		user_data: *mut c_void,
	) -> Self {
		Self {
			callback,
			user_data,
		}
	}
}

impl core::fmt::Write for UserWriter {
	fn write_str(&mut self, source: &str) -> core::fmt::Result {
		for byte in source.bytes() {
			// SAFETY: we of course assume the soundness of the C implementation of `callback`.
			// Beyond that, we have upheld our guarantee to only pass `self.user_data` as the first parameter.
			unsafe {
				(self.callback)(self.user_data, byte as c_char);
			}
		}
		Ok(())
	}
}

/// Writes to the provided buffer, truncating if it runs out of bytes.
pub struct BytesWriter<'a> {
	pub bytes: &'a mut [u8],
}

impl<'a> BytesWriter<'a> {
	#[must_use]
	pub fn new(bytes: &'a mut [u8]) -> Self {
		Self { bytes }
	}
}

impl core::fmt::Write for BytesWriter<'_> {
	fn write_str(&mut self, source: &str) -> core::fmt::Result {
		let source = source.as_bytes();
		let len = source.len().min(self.bytes.len());
		self.bytes.copy_from_slice(&source[..len]);
		// We use `core::mem::take` here to work around a sticky lifetime issue:
		// We have two borrows involved: `&'self mut self` and `&'bytes mut [u8]`.
		// Thus, `&mut self.bytes` is `&'self mut &'bytes mut [u8]`.
		// Dereferencing this borrow (which using `self.bytes` as a value would implicitly do due to place semantics) only gives us `&'self mut [u8]`!
		// (This concept--getting a shorter "view" of a longer borrow--is called reborrowing.)
		// Instead, we need to "take" the reference away from the `self.bytes` field, so we can get the actual `&'bytes mut [u8]`.
		// (This situation never occurs with immutable borrows because they are `Copy` so with `&'a &'b T` one can simply copy the `&'b T` out.)
		// This is very subtle!! Good thing the compiler caught it for us before we caused memory unsoundness!
		self.bytes = &mut core::mem::take(&mut self.bytes)[len..];
		Ok(())
	}
}
