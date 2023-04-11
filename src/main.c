#include <sqlite3.h>

#include "devices/aht20.h"
#include "devices/ds3231.h"
#include "devices/lcd.h"
#include "emmc.h"
#include "gpio.h"
#include "gpt.h"
#include "i2c.h"
#include "random.h"
#include "sleep.h"
#include "sqlite-api.h"
#include "time.h"
#include "timer.h"
#include "try.h"
#include "uart.h"

// #define STUB_SQLITE

#ifndef STUB_SQLITE
// 93c77e5d-5bc5-439d-885c-cfdfa9cadce5
static guid_t const expected_guid = { 0x93c77e5d, 0x5bc5, 0x439d, { 0x88, 0x5c }, { 0xcf, 0xdf, 0xa9, 0xca, 0xdc, 0xe5 } };
#endif

static u8 const DEGREE_DATA[8] = {
	0b000'00110, 0b000'01001, 0b000'01001, 0b000'00110, 0, 0, 0, 0,
};

enum : char { DEGREE_CHARACTER = '\1' };

#ifndef STUB_SQLITE
static sqlite3* database = NULL;

// I hope this is enough...
static u8 sqlite_memory[10'000'000] __attribute__((aligned(16)));

static struct statements {
	sqlite3_stmt* average;
	sqlite3_stmt* minimum;
	sqlite3_stmt* maximum;
	sqlite3_stmt* insert;
} statements;
#endif

static int record_to_database(struct time_components const* const time_, struct aht20_data const* const sensor_data) {
#ifndef STUB_SQLITE
	int ret;

	time_t const time = time_unix_from_components(time_);

	sqlite3_stmt* statement = statements.insert;

	ret = sqlite3_reset(statement);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_bind_int64(statement, 1, time);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_bind_double(statement, 2, (f64)sensor_data->temperature);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_bind_double(statement, 3, (f64)sensor_data->humidity);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_step(statement);
	if (ret != SQLITE_DONE) {
		return ret;
	}
#else
	(void)sensor_data;
	(void)time_;
	LOG_DEBUG("would be recorded to database");
#endif

	return SQLITE_OK;
}

enum : u64 { LCD_SLEEP = 1'000 };

// `prefix` can be at most 4 characters.
static void show_time_components(struct time_components const* const time, char const* const prefix) {
	lcd_printf_all(
		"%4.4s %2hhu %3.3s %04i\n%2hhu:%02hhu:%02hhu",
		prefix,
		time->day_of_month,
		time_month_to_string_short(time->month),
		time->year,
		time->hour,
		time->minute,
		time->second);
}

enum widget_response : u8 {
	wr_none,
	wr_cancel,
	wr_submit,
};

struct vertical_menu {
	u8 selected;
	u8 first_shown;
};

[[nodiscard]] static enum widget_response update_vertical_menu(struct vertical_menu* const this, u8 const last, lcd_buttons_t const buttons) {
	if (buttons & lcd_button_down && this->selected < last) {
		u8 const last_shown = this->first_shown + LCD_LINES - 1;
		if (this->selected == last_shown) {
			++this->first_shown;
		}
		++this->selected;
	} else if (buttons & lcd_button_up && this->selected > 0) {
		if (this->selected == this->first_shown) {
			--this->first_shown;
		}
		--this->selected;
	}

	if (buttons & lcd_button_right || buttons & lcd_button_select) {
		return wr_submit;
	} else if (buttons & lcd_button_left) {
		return wr_cancel;
	} else {
		return wr_none;
	}
}

static void show_vertical_menu(struct vertical_menu* const this, u8 const last, char const* const names[]) {
	for (u8 line = 0; line < LCD_LINES; ++line) {
		u8 const option = this->first_shown + line;
		lcd_set_position(line, 0);
		if (option <= last) {
			lcd_printf("%c %s", option == this->selected ? '>' : ' ', names[option]);
		} else {
			lcd_print_line("");
		}
	}
}

enum display_mode_kind : u8 {
	display_time,
	display_sensor,
	display_menu,
	display_query,
	display_set_time,
	display_db_was_reset,
};

enum focused_time_component : u8 {
	focused_day,
	focused_month,
	focused_year,
	focused_hour,
	focused_minute,
	focused_second,
#define focused_LAST focused_second
};

struct time_editor {
	struct time_components time;
	enum focused_time_component focused;
	u8 _pad0[3];
};

[[nodiscard]] static enum widget_response update_time_editor(struct time_components* const time, enum focused_time_component* const focused, lcd_buttons_t const new_buttons) {
	if (new_buttons & lcd_button_up || new_buttons & lcd_button_down) {
		i8 const offset = new_buttons & lcd_button_up ? 1 : -1;
		switch (*focused) {
			case focused_day: {
				u8 const days_in_month = time_days_in_month(time->month, time_is_leap_year(time->year));
				time->day_of_month += offset;
				if (time->month > days_in_month + 1) {
					time->month = 1;
				} else if (time->month == 0) {
					time->month = days_in_month;
				}
			} break;
			case focused_month: {
				time->month += offset;
				if (time->month > MONTHS_PER_YEAR) {
					time->month = offset > 0 ? 0 : MONTHS_PER_YEAR - 1;
				}
			} break;
			case focused_year: {
				time->year += offset;
				if (time->year < DS3231_YEAR_MIN) {
					time->year = DS3231_YEAR_MIN;
				} else if (time->year > DS3231_YEAR_MAX) {
					time->year = DS3231_YEAR_MAX;
				}
			} break;
			case focused_hour: {
				time->hour += offset;
				if (time->hour > HOURS_PER_DAY) {
					time->hour = offset > 0 ? 0 : HOURS_PER_DAY - 1;
				}
			} break;
			case focused_minute: {
				time->minute += offset;
				if (time->minute > MINUTES_PER_HOUR) {
					time->minute = offset > 0 ? 0 : MINUTES_PER_HOUR - 1;
				}
			} break;
			case focused_second: {
				time->second += offset;
				if (time->second > SECONDS_PER_MINUTE) {
					time->second = offset > 0 ? 0 : SECONDS_PER_MINUTE - 1;
				}
			} break;
		}
	} else if (new_buttons & lcd_button_left || new_buttons & lcd_button_right) {
		i8 const offset = new_buttons & lcd_button_right ? 1 : -1;
		if (*focused == 0 && offset < 0) {
			return wr_cancel;
		} else if (*focused == focused_LAST && offset > 0) {
			return wr_submit;
		} else {
			*focused = (u8)(*focused + offset) % (focused_LAST + 1);
		}
	} else if (new_buttons & lcd_button_select) {
		return wr_submit;
	}
	return wr_none;
}

static void show_time_editor(struct time_components const* const time, enum focused_time_component const focused, char const* const prefix) {
	// clang-format off
	// For god's sake stop adding alignment!
	static lcd_position_t const COMPONENT_POSITIONS[focused_LAST + 1] = {
		[focused_day] = { 0, 6 }, [focused_month] = { 0, 10 }, [focused_year] = { 0, 15 },
		[focused_hour] = { 1, 1 }, [focused_minute] = { 1, 4 }, [focused_second] = { 1, 7 },
	};
	// clang-format on

	show_time_components(time, prefix);

	lcd_position_t const position = COMPONENT_POSITIONS[focused];
	lcd_set_position(position.line, position.column);
}

enum menu_option : u8 {
	menu_query,
	menu_set_time,
	menu_reset_db,
	menu_off,
#define menu_LAST menu_off
};

union display_mode_state {
	struct display_mode_state_time {
	} time;
	struct display_mode_state_sensor {
	} sensor;
	struct display_mode_state_menu {
		struct vertical_menu menu;
	} menu;
	struct display_mode_state_query {
		struct time_components start, end;
		union {
			struct vertical_menu menu;
			enum focused_time_component focused_time;
			struct {
				struct aht20_data data;
				enum query_status : u8 { query_status_ok, query_status_error, query_status_no_data } status;
				u8 _pad0[3];
			} results;
		} editor_or_results;
		enum query_type : u8 {
			query_type_average,
			query_type_minimum,
			query_type_maximum,
#define query_type_LAST query_type_maximum
		} type;
		enum query_stage : u8 {
			query_stage_type,
			query_stage_start,
			query_stage_end,
			query_stage_results,
		} stage;
		u8 _pad0[2];
	} query;
	struct display_mode_state_set_time {
		struct time_components time;
		enum focused_time_component focused;
		u8 _pad0[3];
	} set_time;
	struct display_mode_state_db_was_reset {
		bool success;
	} db_was_reset;
};

static char const* query_type_NAMES[query_type_LAST + 1] = {
	[query_type_average] = "Average",
	[query_type_minimum] = "Minimum",
	[query_type_maximum] = "Maximum",
};

static int make_query(struct display_mode_state_query* const state) {
	int ret;

	sqlite3_stmt* statement;

	state->stage = query_stage_results;
	state->editor_or_results.results.status = query_status_error;

	switch (state->type) {
		case query_type_average:
			statement = statements.average;
			break;
		case query_type_minimum:
			statement = statements.minimum;
			break;
		case query_type_maximum:
			statement = statements.maximum;
			break;
	}

	time_t const start = time_unix_from_components(&state->start);
	time_t const end = time_unix_from_components(&state->end);

	ret = sqlite3_reset(statement);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_bind_int64(statement, 1, start);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_bind_int64(statement, 2, end);
	if (ret != SQLITE_OK) {
		return ret;
	}

	ret = sqlite3_step(statement);
	if (ret != SQLITE_ROW) {
		return ret;
	}

	if (sqlite3_column_type(statement, 0) == SQLITE_NULL || sqlite3_column_type(statement, 1) == SQLITE_NULL) {
		state->editor_or_results.results.status = query_status_no_data;
	} else {
		state->editor_or_results.results.status = query_status_ok;
		state->editor_or_results.results.data = (struct aht20_data){
			.temperature = (f32)sqlite3_column_double(statement, 0),
			.humidity = (f32)sqlite3_column_double(statement, 1),
		};
	}

	return SQLITE_OK;
}

struct display_mode {
	union display_mode_state state;
	enum display_mode_kind kind;
	u8 _pad0[3];
};

#define SET_MODE(_mode, ...) (display_mode_.kind = display_##_mode, display_mode_.state._mode = (struct display_mode_state_##_mode)__VA_ARGS__)

static struct time_components offset_components(struct time_components const* const from, i64 const offset_seconds) {
	time_t const from_unix = time_unix_from_components(from);
	time_t const to_unix = from_unix + offset_seconds;
	struct time_components to = *from;
	time_unix_to_components(to_unix, &to);
	return to;
}

static char const* menu_NAMES[menu_LAST + 1] = {
	[menu_query] = "Query",
	[menu_set_time] = "Set Time",
	[menu_reset_db] = "Reset DB",
	[menu_off] = "Off",
};

void main(void) {
	int ret;

	i2c_init(i2c_speed_standard);
	sleep_micros(100'000);

	assert(aht20_init(), "initializing AHT20");

	lcd_init();
	lcd_load_character(DEGREE_CHARACTER, DEGREE_DATA);
	lcd_clear();

#ifndef STUB_SQLITE
	assert(emmc_init(), "initializing EMMC");
	assert(find_partition_by_guid(&expected_guid, &sqlite_database_partition), "could not find database partition on SD card; check the GUID");

	sqlite3_config(SQLITE_CONFIG_HEAP, sqlite_memory, sizeof(sqlite_memory), 512);
	ret = sqlite3_initialize();
	assert(ret == SQLITE_OK, "sqlite init failed with code %d", ret);
	ret = sqlite3_open_v2("maindb", &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "mine");
	assert(ret == SQLITE_OK, "sqlite open failed with code %d", ret);
	ret = sqlite3_exec(database, "pragma journal_mode=off", NULL, NULL, NULL);
	assert(ret == SQLITE_OK, "sqlite journal pragma failed with code %d", ret);
	char* msg = NULL;
	ret = sqlite3_exec(
		database, "create table if not exists data (time integer primary key, temperature real not null, humidity real not null) without rowid, strict", NULL, NULL, &msg);
	assert(ret == SQLITE_OK, "sqlite create failed with code %d, msg %s", ret, msg);
	sqlite3_free(msg);

	ret = sqlite3_prepare_v2(database, "select avg(temperature), avg(humidity) from data where time > ?1 and time < ?2", -1, &statements.average, NULL);
	assert(ret == SQLITE_OK, "sqlite prepare (average) failed with code %d", ret);
	ret = sqlite3_prepare_v2(database, "select min(temperature), min(humidity) from data where time > ?1 and time < ?2", -1, &statements.minimum, NULL);
	assert(ret == SQLITE_OK, "sqlite prepare (minimum) failed with code %d", ret);
	ret = sqlite3_prepare_v2(database, "select max(temperature), max(humidity) from data where time > ?1 and time < ?2", -1, &statements.maximum, NULL);
	assert(ret == SQLITE_OK, "sqlite prepare (maximum) failed with code %d", ret);
	ret = sqlite3_prepare_v2(database, "insert into data values (?1, ?2, ?3)", -1, &statements.insert, NULL);
	assert(ret == SQLITE_OK, "sqlite prepare (insert) failed with code %d", ret);
#endif

	struct aht20_data sensor_data;
	bool sensor_error = false;

	struct time_components real_time;
	bool time_error = false;

	u64 measure_next = timer_get_micros();
	u8 last_minute_recorded = 255;

	struct display_mode display_mode_;
	SET_MODE(time, {});

	lcd_buttons_t old_buttons = 0;
	lcd_buttons_t raw_buttons = lcd_get_buttons();

	while (true) {
		// Get these now to minimize latency.
		lcd_buttons_t new_buttons = raw_buttons & ~old_buttons;
		old_buttons = raw_buttons;

		u64 const this_time = timer_get_micros();
		if (this_time >= measure_next) {
			// Make sure that the next step will actually be in the future.
			// (This method is actually more optimal than an algebraic solution, since we expect this loop to execute once or maybe twice.)
			do {
				measure_next += 5'000'000;
			} while (this_time >= measure_next);

			time_error = !ds3231_get_time(&real_time);
			if (time_error) {
				LOG_ERROR("failed to get time from ds3231");
			}

			sleep_micros(1'000);

			sensor_error = !aht20_measure(&sensor_data);
			if (sensor_error) {
				LOG_ERROR("failed to get data from aht20");
			}

			// Also use this timing handler to alternate the UI.
			switch (display_mode_.kind) {
				case display_time:
					SET_MODE(sensor, {});
					break;
				case display_db_was_reset:
				case display_sensor:
					SET_MODE(time, {});
					break;
				default:
					break;
			}
		}

		// Record data in database only once a minute.
		if (real_time.minute != last_minute_recorded) {
			if (time_error || sensor_error) {
				LOG_ERROR("database entry skipped due to time/data error");
			} else {
				last_minute_recorded = real_time.minute;
				LOG_INFO(
					"recording to database for %04i-%02hhu-%02hhu %02hhu:%02hhu", real_time.year, real_time.month + 1, real_time.day_of_month, real_time.hour, real_time.minute);

				ret = record_to_database(&real_time, &sensor_data);
				if (ret != SQLITE_OK) {
					LOG_ERROR("recording to database failed with code %d", ret);
				}
			}
		}

		// Handle input.
		switch (display_mode_.kind) {
			case display_time:
			case display_sensor: {
				if (new_buttons & lcd_button_select) {
					SET_MODE(menu, { .menu = { 0 } });
				}
			} break;
			case display_menu: {
				switch (update_vertical_menu(&display_mode_.state.menu.menu, menu_LAST, new_buttons)) {
					case wr_submit:
						switch ((enum menu_option)display_mode_.state.menu.menu.selected) {
							case menu_query:
								SET_MODE(
									query,
									{
										.stage = query_stage_type,
										.type = 0,
										.start = offset_components(&real_time, -7 * 24 * 60 * 60),  // Go one week before by default.
										.end = real_time,
										.editor_or_results = { .menu = {} },
									});
								break;
							case menu_set_time:
								SET_MODE(set_time, { .time = real_time, .focused = 0 });
								break;
							case menu_reset_db:
#ifdef STUB_SQLITE
								ret = SQLITE_OK;
#else
								ret = sqlite3_exec(database, "DELETE FROM data", NULL, NULL, NULL);
#endif
								if (ret != SQLITE_OK) {
									LOG_ERROR("`DELETE FROM data` failed with code %d", ret);
								}
								SET_MODE(db_was_reset, { .success = ret == SQLITE_OK });
								break;
							case menu_off:
								goto break_outer;
						}
						break;
					case wr_cancel:
						SET_MODE(time, {});
						break;
					case wr_none:
						break;
				}
			} break;
			case display_query: {
				switch (display_mode_.state.query.stage) {
					case query_stage_type: {
						switch (update_vertical_menu(&display_mode_.state.query.editor_or_results.menu, query_type_LAST, new_buttons)) {
							case wr_submit:
								display_mode_.state.query.type = display_mode_.state.query.editor_or_results.menu.selected;
								display_mode_.state.query.stage = query_stage_start;
								display_mode_.state.query.editor_or_results.focused_time = 0;
								break;
							case wr_cancel:
								SET_MODE(time, {});
								break;
							case wr_none:
								break;
						}
						break;
						case query_stage_start: {
							switch (update_time_editor(&display_mode_.state.query.start, &display_mode_.state.query.editor_or_results.focused_time, new_buttons)) {
								case wr_submit:
									display_mode_.state.query.stage = query_stage_end;
									display_mode_.state.query.editor_or_results.focused_time = 0;
									break;
								case wr_cancel:
									display_mode_.state.query.stage = query_stage_type;
									display_mode_.state.query.editor_or_results.menu.first_shown = display_mode_.state.query.editor_or_results.menu.selected
										= display_mode_.state.query.type;
									break;
								case wr_none:
									break;
							}
						} break;
						case query_stage_end: {
							switch (update_time_editor(&display_mode_.state.query.end, &display_mode_.state.query.editor_or_results.focused_time, new_buttons)) {
								case wr_submit: {
									ret = make_query(&display_mode_.state.query);
									if (ret != SQLITE_OK) {
										LOG_ERROR("making query failed with code %d", ret);
									}
								} break;
								case wr_cancel:
									display_mode_.state.query.stage = query_stage_start;
									display_mode_.state.query.editor_or_results.focused_time = 0;
									break;
								case wr_none:
									break;
							}
						} break;
						case query_stage_results: {
							if (new_buttons & lcd_button_right || new_buttons & lcd_button_select) {
								SET_MODE(time, {});
							} else if (new_buttons & lcd_button_left) {
								display_mode_.state.query.stage = query_stage_end;
								display_mode_.state.query.editor_or_results.focused_time = 0;
							}
						} break;
					}
				}
			} break;
			case display_set_time: {
				switch (update_time_editor(&display_mode_.state.set_time.time, &display_mode_.state.set_time.focused, new_buttons)) {
					case wr_none:
						break;
					case wr_submit:
						ds3231_set_time(&display_mode_.state.set_time.time);
						real_time = display_mode_.state.set_time.time;
						[[fallthrough]];
					case wr_cancel:
						SET_MODE(time, {});
						break;
				}
			} break;
			case display_db_was_reset: {
				if (new_buttons & lcd_button_select) {
					SET_MODE(time, {});
				}
				// Will also be exited by the timing handler.
			} break;
		}

		bool cursor = false;
		char const* failed = NULL;

		// Render.
		switch (display_mode_.kind) {
			case display_time:
				if (time_error) {
					failed = "time";
				} else {
					show_time_components(&real_time, "");
				}
				break;
			case display_sensor:
				if (sensor_error) {
					failed = "sensor";
				} else {
					lcd_printf_all("%.1f%cC %.1f%%\npress Select", (f64)sensor_data.temperature, DEGREE_CHARACTER, (f64)sensor_data.humidity);
				}
				break;
			case display_menu: {
				show_vertical_menu(&display_mode_.state.menu.menu, menu_LAST, menu_NAMES);
			} break;
			case display_query: {
				switch (display_mode_.state.query.stage) {
					case query_stage_type: {
						show_vertical_menu(&display_mode_.state.query.editor_or_results.menu, query_type_LAST, query_type_NAMES);
					} break;
					case query_stage_start: {
						show_time_editor(&display_mode_.state.query.start, display_mode_.state.query.editor_or_results.focused_time, "From");
						cursor = true;
					} break;
					case query_stage_end: {
						show_time_editor(&display_mode_.state.query.end, display_mode_.state.query.editor_or_results.focused_time, "To");
						cursor = true;
					} break;
					case query_stage_results: {
						bool no_data = false;
						switch (display_mode_.state.query.editor_or_results.results.status) {
							case query_status_error:
								failed = "query";
								break;
							case query_status_no_data:
								no_data = true;
								[[fallthrough]];
							case query_status_ok: {
								char const* name = NULL;
								switch (display_mode_.state.query.type) {
									case query_type_maximum:
										name = "Maximum";
										break;
									case query_type_minimum:
										name = "Minimum";
										break;
									case query_type_average:
										name = "Average";
										break;
								}

								if (no_data) {
									lcd_printf_all("%s:\nNo data!", name);
								} else {
									lcd_printf_all(
										"%s:\n%.1f%cC %.1f%%",
										name,
										(f64)display_mode_.state.query.editor_or_results.results.data.temperature,
										DEGREE_CHARACTER,
										(f64)display_mode_.state.query.editor_or_results.results.data.humidity);
								}
							} break;
						}
					} break;
				}
			} break;
			case display_set_time: {
				show_time_editor(&display_mode_.state.set_time.time, display_mode_.state.set_time.focused, "Set");
				cursor = true;
			} break;
			case display_db_was_reset: {
				if (display_mode_.state.db_was_reset.success) {
					lcd_printf_all("DB was reset");
				} else {
					failed = "reset DB";
				}
			} break;
		}

		lcd_set_display(true, cursor, cursor);
		if (failed == NULL) {
			lcd_set_backlight(true, true, true);
		} else {
			lcd_set_backlight(true, false, false);
			lcd_printf_all("%.10s error", failed);
		}

		// Wait for something we can react to.
		while (timer_get_micros() < measure_next && (raw_buttons = lcd_get_buttons()) == old_buttons) {
			sleep_micros(500);
		}
	}
break_outer:;

#ifndef STUB_SQLITE
	sqlite3_finalize(statements.average);
	sqlite3_finalize(statements.minimum);
	sqlite3_finalize(statements.maximum);
	sqlite3_finalize(statements.insert);
	sqlite3_close(database);
	sqlite3_shutdown();
	sleep_micros(10'000);
#endif

	lcd_set_backlight(true, false, true);
	lcd_printf_all("System now idle\nCan be unplugged");
}
