use core::ffi::c_char;
use core::mem::MaybeUninit;
use core::str::FromStr;

unsafe fn inner<T: FromStr>(source: *const c_char, source_len: usize) -> Option<T> {
	let source = core::slice::from_raw_parts(source.cast(), source_len);
	let source = core::str::from_utf8(source).ok()?;
	source.parse().ok()
}

unsafe fn implementation<T: FromStr>(
	source: *const c_char,
	source_len: usize,
	valid: *mut bool,
) -> MaybeUninit<T> {
	let res = inner(source, source_len);
	if !valid.is_null() {
		unsafe { *valid = res.is_some() };
	}
	res.map_or(MaybeUninit::uninit(), MaybeUninit::new)
}

macro_rules! make_fn {
	($name:ident, $ty:ty) => {
		#[no_mangle]
		pub unsafe extern "C" fn $name(
			source: *const c_char,
			source_len: usize,
			valid: *mut bool,
		) -> MaybeUninit<$ty> {
			implementation(source, source_len, valid)
		}
	};
}

make_fn!(parse_i64, i64);
make_fn!(parse_u64, u64);
make_fn!(parse_f64, f64);
