use core::ffi::c_char;
use core::fmt::Write as _;

use crate::writer::BytesWriter;

/// A stack-allocated buffer that can store a C string.
///
/// For a given `LEN`, you can store up to `LEN - 1` actual characters, due to the NUL terminator.
pub struct SmallCString<const LEN: usize>([u8; LEN]);

// Used for clarity.
const NUL: u8 = b'\0';

impl<const LEN: usize> SmallCString<LEN> {
	/// Convert directly from a byte slice.
	///
	/// Note that `&str` can be used with this too; just use `str::as_bytes`.
	///
	/// This will truncate the string to `LEN - 1` actual characters.
	/// If there are embedded NUL bytes in `bytes`, the resulting C string will end at the first one.
	pub fn new(rust: &[u8]) -> Self {
		// This is an example of iterators in Rust.
		// Iterators can be amazingly expressive,
		// and also optimize as well if not better then typical, imperative equivalents, mainly due to avoiding bounds checks.
		let mut iter = rust.iter().copied().take(LEN - 1);
		// This will zero-fill the rest of the buffer even though C only needs one NUL terminator.
		// We could avoid this with `unsafe` code, but it's not really worth it.
		let storage = core::array::from_fn(|_| iter.next().unwrap_or(NUL));
		Self(storage)
	}

	/// Write the format arguments `args` into a new instance, without allocating, of course.
	// It's good practice to document any panics like so:
	/// # Panics
	///
	/// If any formatting implementations in `args` return an `Err`.
	pub fn from_format(args: core::fmt::Arguments<'_>) -> Self {
		let mut storage = [NUL; LEN];
		// Make sure we leave at least one byte for a NUL terminator in all cases.
		// This may write internal NUL bytes, but this is not an issue: the C string will just end early.
		BytesWriter::new(&mut storage[..LEN - 1])
			.write_fmt(args)
			.unwrap();
		Self(storage)
	}

	/// Get the actual C string!
	/// The returned pointer points to a region of `LEN` bytes which is guaranteed to contain a NUL terminator.
	///
	/// The storage of the returned pointer only lasts as long as the instance, so be careful.
	pub fn as_c(&self) -> *const c_char {
		const _: () = {
			assert!(core::mem::size_of::<u8>() == core::mem::size_of::<c_char>());
		};
		self.0.as_ptr().cast()
	}
}

// A `From` implementation allows conversions via `from` and `into`.
impl<const LEN: usize, T: AsRef<[u8]>> From<T> for SmallCString<LEN> {
	fn from(rust: T) -> Self {
		Self::new(rust.as_ref())
	}
}

/// A convenience alias for `SmallCString::new`.
pub fn cstr<const LEN: usize>(rust: impl AsRef<[u8]>) -> SmallCString<LEN> {
	rust.into()
}
