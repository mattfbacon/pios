// `c_char` is equivalent to C's `char` and is more correct than `u8` or `i8` since the signedness of `char` is implementation-defined.
// `c_void` is equivalent to C's `void` when used behind a pointer: `*mut c_void` in Rust is `void*` in C.
// (C's `void` as in the return type of functions is equivalent to Rust's unit type `()`.)
use core::ffi::{c_char, c_void};
use core::fmt::rt::v1::{Alignment, Argument, Count, FormatSpec};
use core::fmt::{ArgumentV1, Arguments, UnsafeArg, Write as _};

use crate::writer::UserWriter;

/// Since this struct will also exist on the C side, we mark it as `#[repr(C)]` to allow safe inter-operation.
#[repr(C)]
pub struct Args {
	pub width: u32,
	pub precision: u32,
	pub alternate: bool,
	pub zero_padded: bool,
	pub left_justified: bool,
	pub always_sign: bool,
}

const FLAG_PLUS: u32 = 1 << 0;
// The `-` flag, `1 << 1`, is unused.
const FLAG_ALTERNATE: u32 = 1 << 2;
const FLAG_ZERO: u32 = 1 << 3;

fn to_count(raw: u32) -> Count {
	if raw == u32::MAX {
		Count::Implied
	} else {
		Count::Is(raw.try_into().unwrap())
	}
}

fn implementation(mut writer: UserWriter, placeholder: ArgumentV1<'_>, args: &Args) {
	let pieces = &["", ""];

	let placeholders = &[placeholder];

	// To avoid making `flags` mutable in the outer scope, we use a block, which creates a new scope, for the initialization.
	// The final expression without a semicolon, `flags`, is the value of the whole block, which we assign to `flags` in the outer scope.
	let flags = {
		let mut flags = 0;
		if args.alternate {
			flags |= FLAG_ALTERNATE;
		}
		if args.zero_padded {
			flags |= FLAG_ZERO;
		}
		if args.always_sign {
			flags |= FLAG_PLUS;
		}
		flags
	};

	let align = if args.left_justified {
		Alignment::Left
	} else {
		Alignment::Right
	};

	let fmts = &[Argument {
		position: 0,
		format: FormatSpec {
			width: to_count(args.width),
			precision: to_count(args.precision),
			flags,
			fill: ' ',
			align,
		},
	}];

	// SAFETY:
	// - `pieces` (2 items) is at least as long as `placeholders` (1 item).
	// - All positions in `fmts` point to valid indices of `placeholders`: `placeholders` has one item and our only position is `0`.
	// - There are no count parameters in `fmts`, so they are all vacuously valid.
	let args = Arguments::new_v1_formatted(pieces, placeholders, fmts, unsafe { UnsafeArg::new() });

	writer.write_fmt(args).unwrap();
}

// We use a macro to declare the actual C API.
// This allows avoiding excessive repetition of code.
macro_rules! make_fn {
	($name:ident, $ty:ty, $format_trait:ident) => {
		/// Writes `value` in a format configured by `args` to the writer described by `callback` and `user_data`.
		///
		/// # Safety
		///
		/// `callback` must be sound to call with `user_data` as the first argument.
		#[no_mangle]
		pub unsafe extern "C" fn $name(
			callback: unsafe extern "C" fn(*mut c_void, c_char),
			user_data: *mut c_void,
			value: $ty,
			args: &Args,
		) {
			// SAFETY: `callback` and `user_data` are provided from C and we assume that they are valid.
			let writer = unsafe { UserWriter::new(callback, user_data) };
			let placeholder = ArgumentV1::new(&value, core::fmt::$format_trait::fmt);
			implementation(writer, placeholder, args);
		}
	};
}

// There are three "families":
// Simple decimal conversions,
make_fn!(fmt_u64, u64, Display);
make_fn!(fmt_i64, i64, Display);
// special bases for unsigned integers,
make_fn!(fmt_u64_hex, u64, LowerHex);
make_fn!(fmt_u64_oct, u64, Octal);
make_fn!(fmt_u64_bin, u64, Binary);
// and floats.
make_fn!(fmt_f64, f64, Display);
make_fn!(fmt_f64_exp, f64, LowerExp);
