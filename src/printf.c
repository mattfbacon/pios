#include "log.h"
#include "math.h"
#include "printf.h"
#include "string.h"

struct fmt_args {
	u32 width, precision;
	bool alternate, zero_padded, left_justified, always_sign;
};

void fmt_u64(printf_write_callback_t callback, void* user_data, u64 value, struct fmt_args const* args);
void fmt_i64(printf_write_callback_t callback, void* user_data, i64 value, struct fmt_args const* args);

void fmt_u64_hex(printf_write_callback_t callback, void* user_data, u64 value, struct fmt_args const* args);
void fmt_u64_oct(printf_write_callback_t callback, void* user_data, u64 value, struct fmt_args const* args);
void fmt_u64_bin(printf_write_callback_t callback, void* user_data, u64 value, struct fmt_args const* args);

void fmt_f64(printf_write_callback_t callback, void* user_data, f64 value, struct fmt_args const* args);
void fmt_f64_exp(printf_write_callback_t callback, void* user_data, f64 value, struct fmt_args const* args);

static u64 str_to_u64(char const* const source, char const** const out_end) {
	char const* end = source;
	while (isdigit(*end)) {
		++end;
	}
	bool valid;
	u64 const ret = parse_u64(source, (usize)(end - source), &valid);
	*out_end = end;
	// If it's not valid but only received digits, it must have overflowed.
	// The largest possible value will suffice.
	return valid ? ret : U64_MAX;
}

static void write_str(printf_write_callback_t const write, void* const user, char const* str, usize max_length) {
	for (; max_length > 0 && *str != '\0'; ++str, --max_length) {
		write(user, *str);
	}
}

static void write_bytes_hex(printf_write_callback_t const write, void* const user, u8 const* const buf, usize const length) {
	char itoa_buf[2];
	for (usize i = 0; i < length; ++i) {
		u8_to_str_hex(itoa_buf, buf[i]);
		write_str(write, user, itoa_buf, sizeof(itoa_buf));
	}
}

isize dprintf(printf_write_callback_t const write, void* user, char const* fmt, ...) {
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	isize const ret = vdprintf(write, user, fmt, args);
	__builtin_va_end(args);
	return ret;
}

enum format_padding {
	format_padding_left_justified,
	format_padding_right_justified,
	format_padding_zero_padded,
};

enum format_sign {
	format_sign_default,
	format_sign_blank,
	format_sign_always,
};

_Static_assert(sizeof(long) == sizeof(long long));

enum format_length {
	format_length_hh,
	format_length_h,
	format_length_default,
	// Or ll or q or L or j or z or Z or t.
	format_length_l,
};

enum format_conversion {
	// Standard:
	// Or i.
	format_conversion_d,
	format_conversion_u,
	format_conversion_o,
	format_conversion_x,
	format_conversion_b,
	format_conversion_e,
	format_conversion_f,
	format_conversion_g,
	format_conversion_c,
	format_conversion_s,
	format_conversion_p,
	format_conversion_n,
	format_conversion_literal_percent,

	// Our additions:
	format_conversion_data,
	format_conversion_boolean,
};

struct format_specifier {
	u32 width;
	// `U32_MAX` means none specified (or the user specified that value; same result).
	u32 precision;

	enum format_padding padding;
	enum format_sign sign;

	enum format_length length_modifier;

	enum format_conversion conversion;
	bool conversion_capital;

	bool alternate;

	bool width_dynamic;
	bool precision_dynamic;

	bool error;

	u8 _pad0[3];
};

static struct format_specifier parse_format_specifier(char const** const fmt_ptr) {
	char const* fmt = *fmt_ptr;
	struct format_specifier ret = {
		.alternate = false,
		.padding = format_padding_right_justified,
		.sign = format_sign_default,
		.width_dynamic = false,
		.width = 0,
		.precision_dynamic = false,
		.precision = U32_MAX,
		.length_modifier = format_length_default,
		// .conversion = to be set,
		.conversion_capital = false,
		.error = false,
	};

#define ERROR(...) return (LOG_ERROR(__VA_ARGS__), ret.error = true, ret)
#define EOI_CHECK \
	if (*fmt == '\0') { \
		ERROR("unexpected end of input"); \
	}

	EOI_CHECK

	if (*fmt == '$') {
		ERROR("$ specifiers not implemented");
	}

	for (; *fmt != '\0'; ++fmt) {
		switch (*fmt) {
			case '#':
				ret.alternate = true;
				break;
			case '0':
				// `-` overrides `0`.
				if (ret.padding != format_padding_left_justified) {
					ret.padding = format_padding_zero_padded;
				}
				break;
			case '-':
				ret.padding = format_padding_left_justified;
				break;
			case ' ':
				// `+` overrides ` `.
				if (ret.sign != format_sign_always) {
					ret.sign = format_sign_blank;
				}
				break;
			case '+':
				ret.sign = format_sign_always;
				break;
			default:
				goto flags_done;
		}
	}
flags_done:;

	EOI_CHECK

	if (*fmt == '*') {
		ret.width_dynamic = true;
		++fmt;
	} else if (*fmt >= '1' && *fmt <= '9') {
		ret.width = (u32)str_to_u64(fmt, &fmt);
	}

	EOI_CHECK

	if (*fmt == '.') {
		++fmt;
		EOI_CHECK

		if (*fmt == '*') {
			ret.precision_dynamic = true;
			++fmt;
		} else if (*fmt >= '1' && *fmt <= '9') {
			ret.precision = (u32)str_to_u64(fmt, &fmt);
		}
		EOI_CHECK
	}

	switch (*fmt) {
		case 'h': {
			ret.length_modifier = format_length_h;
			++fmt;
			if (*fmt == 'h') {
				ret.length_modifier = format_length_hh;
				++fmt;
			}
		} break;
		case 'q':
		case 'L':
		case 'j':
		case 'z':
		case 'Z':
		case 't': {
			ret.length_modifier = format_length_l;
			++fmt;
		} break;
		case 'l': {
			ret.length_modifier = format_length_l;
			++fmt;
			if (*fmt == 'l') {
				++fmt;
			}
		} break;
	}

	EOI_CHECK

	enum {
		ns_standard,
		ns_ours,
	} conversion_namespace
		= ns_standard;
	// Namespace prefix for our additions so they don't conflict with the standard ones.
	if (*fmt == '@') {
		conversion_namespace = ns_ours;
		++fmt;
	}

	switch (conversion_namespace) {
		case ns_standard:
			switch (*fmt) {
				case 'd':
				case 'i':
					ret.conversion = format_conversion_d;
					break;
				case 'o':
					ret.conversion = format_conversion_o;
					break;
				case 'u':
					ret.conversion = format_conversion_u;
					break;
				case 'x':
				case 'X':
					ret.conversion = format_conversion_x;
					break;
				case 'b':
				case 'B':
					ret.conversion = format_conversion_b;
					break;
				case 'e':
				case 'E':
					ret.conversion = format_conversion_e;
					break;
				case 'f':
				case 'F':
					ret.conversion = format_conversion_f;
					break;
				case 'g':
				case 'G':
					ret.conversion = format_conversion_g;
					break;
				case 'c':
					ret.conversion = format_conversion_c;
					break;
				case 's':
					ret.conversion = format_conversion_s;
					break;
				case 'p':
					ret.conversion = format_conversion_p;
					break;
				case 'n':
					ret.conversion = format_conversion_n;
					break;
				case '%':
					ret.conversion = format_conversion_literal_percent;
					break;
				default:
					ERROR("invalid %s format specifier %c", "standard", *fmt);
			}
			break;
		case ns_ours:
			switch (*fmt) {
				case 'd':
					ret.conversion = format_conversion_data;
					break;
				case 'b':
					ret.conversion = format_conversion_boolean;
					break;
				default:
					ERROR("invalid %s format specifier %c", "ours-namespaced", *fmt);
			}
			break;
	}

	ret.conversion_capital = *fmt <= 'Z';

	++fmt;
	*fmt_ptr = fmt;
	return ret;
#undef EOI_CHECK
#undef ERROR
}

// Returns the actual printed width of the field.
static void write_str_with_padding(printf_write_callback_t const write, void* const user, char const* const str, enum format_padding const padding, usize const width, usize const max_length) {
	usize const base_length = strnlen(str, max_length);

	if (base_length > width) {
		write_str(write, user, str, max_length);
		return;
	}

	usize const padding_needed = width - base_length;

	switch (padding) {
		case format_padding_left_justified: {
			write_str(write, user, str, max_length);
			for (usize i = 0; i < padding_needed; ++i) {
				write(user, ' ');
			}
		} break;
		case format_padding_right_justified: {
			for (usize i = 0; i < padding_needed; ++i) {
				write(user, ' ');
			}
			write_str(write, user, str, max_length);
		} break;
		case format_padding_zero_padded: {
			for (usize i = 0; i < padding_needed; ++i) {
				write(user, '0');
			}
			write_str(write, user, str, max_length);
		} break;
	}
}

struct write_wrapper {
	usize bytes_written;
	printf_write_callback_t inner;
	void* inner_user;
};

static void write_wrapper(void* const user_, char const ch) {
	struct write_wrapper* const user = user_;
	user->inner(user->inner_user, ch);
	++user->bytes_written;
}

static void write_wrapper_upper(void* const user, char const ch) {
	write_wrapper(user, char_to_upper(ch));
}

static void write_wrapper_lower(void* const user, char const ch) {
	write_wrapper(user, char_to_lower(ch));
}

static struct fmt_args specifier_to_args(struct format_specifier const* specifier) {
	return (struct fmt_args){
		.width = specifier->width,
		.precision = specifier->precision,
		.alternate = specifier->alternate,
		.zero_padded = specifier->padding == format_padding_zero_padded,
		.left_justified = specifier->padding == format_padding_left_justified,
		.always_sign = specifier->sign == format_sign_always,
	};
}

isize vdprintf(printf_write_callback_t const write_, void* const user_, char const* fmt, __builtin_va_list args) {
	struct write_wrapper wrapper_user = {
		.bytes_written = 0,
		.inner = write_,
		.inner_user = user_,
	};
	void* const user = &wrapper_user;

	for (; *fmt != '\0';) {
		char const ch = *fmt;
		if (ch != '%') {
			write_wrapper(user, ch);
			++fmt;
			continue;
		}

		++fmt;
		struct format_specifier specifier = parse_format_specifier(&fmt);
		if (specifier.error) {
			return -1;
		}

		if (specifier.width_dynamic) {
			specifier.width = (u32) __builtin_va_arg(args, int);
		}

		if (specifier.precision_dynamic) {
			specifier.precision = (u32) __builtin_va_arg(args, int);
		}

		switch (specifier.conversion) {
			case format_conversion_u:
			case format_conversion_d: {
				bool const is_signed = specifier.conversion == format_conversion_d;

				// We need to be careful about sign-extension here.
				// For example, this behavior may be surprising:
				_Static_assert((u64)(i64)(i8)(u8)255 != 255);
				// This is because `255u8` is interpreted as `-1i8`, then sign-extended to `-1i64`, which is interpreted as `U64_MAX`.
				// On the other hand, this works properly:
				_Static_assert((i64)(u64)(u8)255 == 255);
				// This is because once we've promoted to `u64` the cast to `i64` will not perform any sign-extension so the value will be preserved.

				union {
					i64 signed_;
					u64 unsigned_;
				} value;

				switch (specifier.length_modifier) {
					case format_length_hh: {
						if (is_signed) {
							value.signed_ = (i8) __builtin_va_arg(args, i32);
						} else {
							value.unsigned_ = (u8) __builtin_va_arg(args, u32);
						}
					} break;
					case format_length_h: {
						if (is_signed) {
							value.signed_ = (i16) __builtin_va_arg(args, i32);
						} else {
							value.unsigned_ = (u16) __builtin_va_arg(args, u32);
						}
					} break;
					case format_length_default: {
						if (is_signed) {
							value.signed_ = __builtin_va_arg(args, i32);
						} else {
							value.unsigned_ = __builtin_va_arg(args, u32);
						}
					} break;
					case format_length_l: {
						// Yes, `i64` and `u64` will behave the same here,
						// but accessing an inactive variant in a union is not really a good idea
						// (regardless of whether it's formally Undefined Behavior, which it seems not to be)
						// so let's not do that, and instead trust the optimizer to remove this branch.
						if (is_signed) {
							value.signed_ = __builtin_va_arg(args, i64);
						} else {
							value.unsigned_ = __builtin_va_arg(args, u64);
						}
					} break;
				}

				struct fmt_args const fmt_args = specifier_to_args(&specifier);
				if (is_signed) {
					fmt_i64(write_wrapper, user, value.signed_, &fmt_args);
				} else {
					fmt_u64(write_wrapper, user, value.unsigned_, &fmt_args);
				}
			} break;
			case format_conversion_p:
				specifier.conversion = format_conversion_x;
				specifier.length_modifier = format_length_l;
				specifier.alternate = true;
				[[fallthrough]];
			case format_conversion_x:
			case format_conversion_o:
			case format_conversion_b: {
				u64 value;
				switch (specifier.length_modifier) {
					case format_length_hh: {
						value = (u8) __builtin_va_arg(args, u32);
					} break;
					case format_length_h: {
						value = (u16) __builtin_va_arg(args, u32);
					} break;
					case format_length_default: {
						value = __builtin_va_arg(args, u32);
					} break;
					case format_length_l: {
						value = __builtin_va_arg(args, u64);
					} break;
				}

				void (*fmt_fn)(printf_write_callback_t write, void* user, u64 value, struct fmt_args const* args);
				switch (specifier.conversion) {
					case format_conversion_x:
						fmt_fn = fmt_u64_hex;
						break;
					case format_conversion_o:
						fmt_fn = fmt_u64_oct;
						break;
					case format_conversion_b:
						fmt_fn = fmt_u64_bin;
						break;
					default:
						__builtin_unreachable();
				}

				printf_write_callback_t const write = specifier.conversion_capital ? write_wrapper_upper : write_wrapper_lower;
				struct fmt_args const fmt_args = specifier_to_args(&specifier);

				fmt_fn(write, user, value, &fmt_args);
			} break;
			case format_conversion_e:
			case format_conversion_f:
			case format_conversion_g: {
				if (!specifier.precision_dynamic && specifier.precision == U32_MAX) {
					specifier.precision = 6;
				}
				f64 const value = __builtin_va_arg(args, f64);
				printf_write_callback_t const write = specifier.conversion_capital ? write_wrapper_upper : write_wrapper_lower;
				bool use_exp;
				switch (specifier.conversion) {
					case format_conversion_e:
						use_exp = true;
						break;
					case format_conversion_f:
						use_exp = false;
						break;
					case format_conversion_g:
						use_exp = value < 1e-4 || value > pow_f64_u32(10.0, specifier.precision);
						break;
					default:
						__builtin_unreachable();
				}
				struct fmt_args const fmt_args = specifier_to_args(&specifier);
				(use_exp ? fmt_f64_exp : fmt_f64)(write, user, value, &fmt_args);
			} break;
			case format_conversion_c: {
				if (specifier.length_modifier != format_length_default) {
					LOG_ERROR("invalid length specifier for %%%s", "c");
					return -1;
				}

				char const value = (char)__builtin_va_arg(args, int);
				write_str_with_padding(write_wrapper, user, &value, specifier.padding, specifier.width, 1);

			} break;
			case format_conversion_s: {
				if (specifier.length_modifier != format_length_default) {
					LOG_ERROR("invalid length specifier for %%%s", "s");
					return -1;
				}

				char const* str = __builtin_va_arg(args, char const*);
				write_str_with_padding(write_wrapper, user, str, specifier.padding, specifier.width, specifier.precision == U32_MAX ? USIZE_MAX : specifier.precision);
			} break;
			case format_conversion_n: {
				switch (specifier.length_modifier) {
					case format_length_hh: {
						u8* const arg = __builtin_va_arg(args, u8*);
						*arg = (u8)wrapper_user.bytes_written;
					} break;
					case format_length_h: {
						u16* const arg = __builtin_va_arg(args, u16*);
						*arg = (u16)wrapper_user.bytes_written;
					} break;
					case format_length_default: {
						u32* const arg = __builtin_va_arg(args, u32*);
						*arg = (u32)wrapper_user.bytes_written;
					} break;
					case format_length_l: {
						u64* const arg = __builtin_va_arg(args, u64*);
						*arg = (u64)wrapper_user.bytes_written;
					} break;
				}
			} break;
			case format_conversion_literal_percent: {
				write_wrapper(user, '%');
				++fmt;
			} break;
			case format_conversion_data: {
				if (specifier.length_modifier != format_length_default) {
					LOG_ERROR("invalid length specifier for %%%s", "@d");
					return -1;
				}

				u8 const* const buf = __builtin_va_arg(args, u8 const*);
				usize const length = __builtin_va_arg(args, usize);
				write_bytes_hex(write_wrapper, user, buf, length);
			} break;
			case format_conversion_boolean: {
				if (specifier.length_modifier != format_length_default) {
					LOG_ERROR("invalid length specifier for %%%s", "@b");
					return -1;
				}

				static char const* const MESSAGES[2][2] = {
					{ "f", "t" },
					{ "false", "true" },
				};

				bool const arg = (bool)__builtin_va_arg(args, int);
				write_str_with_padding(write_wrapper, user, MESSAGES[specifier.alternate][arg], specifier.padding, specifier.width, USIZE_MAX);
			} break;
		}
	}

	// Will be negative already if the usize value overflows isize,
	// so the postcondition of "Returns a negative value on error" is satisfied.
	return (isize)wrapper_user.bytes_written;
}
