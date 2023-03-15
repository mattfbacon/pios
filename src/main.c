#include <sqlite3.h>

#include "emmc.h"
#include "gpio.h"
#include "gpt.h"
#include "random.h"
#include "sleep.h"
#include "sqlite-api.h"
#include "timer.h"
#include "try.h"
#include "uart.h"

static int exec_callback(void* first_, int const num_columns, char* values[], char* names[]) {
	bool* const first = first_;
	if (*first) {
		for (int i = 0; i < num_columns; ++i) {
			uart_send_str(names[i]);
			uart_send_str(" | ");
		}
		uart_send_str("\r\n");
		*first = false;
	}

	for (int i = 0; i < num_columns; ++i) {
		uart_send_str(values[i]);
		uart_send_str(" | ");
	}
	uart_send_str("\r\n");

	return SQLITE_OK;
}

static void read_line(char* const buffer, usize const length) {
	usize i = 0;
	while (i < length - 1) {
		char const ch = uart_recv();
		switch (ch) {
			case '\r':
				goto break_loop;
			case '\x7f':
				if (i > 0) {
					--i;
				}
				break;
			default:
				buffer[i] = ch;
				++i;
		}
	}
break_loop:;
	buffer[i] = '\0';
}

static void print_header(sqlite3_stmt* const statement) {
	int const num_columns = sqlite3_data_count(statement);
	for (int i = 0; i < num_columns; ++i) {
		char const* const name = sqlite3_column_name(statement, i);
		assert(name != NULL, "sqlite3_column_name returned NULL");
		if (i != 0) {
			uart_send_str(" | ");
		}
		uart_send_str(name);
	}
	uart_send_str("\r\n");
}

static void print_row(sqlite3_stmt* const statement) {
	int const num_columns = sqlite3_data_count(statement);
	for (int i = 0; i < num_columns; ++i) {
		char const* name = (char const*)sqlite3_column_text(statement, i);
		name = name ? name : "NULL";
		if (i != 0) {
			uart_send_str(" | ");
		}
		uart_send_str(name);
	}
	uart_send_str("\r\n");
}

// 93c77e5d-5bc5-439d-885c-cfdfa9cadce5
static guid_t const expected_guid = { 0x93c77e5d, 0x5bc5, 0x439d, { 0x88, 0x5c }, { 0xcf, 0xdf, 0xa9, 0xca, 0xdc, 0xe5 } };

void set_thingy(u32);

void main(void) {
	assert(emmc_init(), "initializing EMMC");
	assert(find_partition_by_guid(&expected_guid, &sqlite_database_partition), "finding database partition");
	LOG_INFO("sqlite database partition is %u..=%u", sqlite_database_partition.first, sqlite_database_partition.last);

	assert(sqlite3_initialize() == SQLITE_OK, "initializing sqlite");

	sqlite3* database;
	LOG_DEBUG("opening the database");
	assert(sqlite3_open_v2("maindb", &database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, "mine") == SQLITE_OK, "opening database");
	LOG_DEBUG("database is open");
	assert(database != NULL, "database is not null");

	sqlite3_exec(database, "PRAGMA journal_mode=OFF", NULL, NULL, NULL);

	while (true) {
		uart_send_str("> ");

		char statement_buf[1024];
		read_line(statement_buf, sizeof(statement_buf));

		if (statement_buf[0] == '\0') {
			break;
		}

		// bool first = true;
		// char* error_message = NULL;
		// int const ret = sqlite3_exec(database, statement_buf, exec_callback, &first, &error_message);
		// if (ret != SQLITE_OK) {
		// 	uart_printf("failed with error %d: %s\r\n", ret, error_message != NULL ? error_message : "(no message)");
		// }
		// if (error_message != NULL) {
		// 	sqlite3_free(error_message);
		// }
		LOG_DEBUG("preparing statement %s", statement_buf);

		sqlite3_stmt* statement = NULL;
		if (sqlite3_prepare_v3(database, statement_buf, -1, 0, &statement, NULL) != SQLITE_OK) {
			LOG_ERROR("preparing statement: %s", sqlite3_errmsg(database));
			sqlite3_finalize(statement);
			continue;
		}
		assert(statement != NULL, "statement is not null");

		LOG_DEBUG("beginning to step");

		bool first = true;
		int step_ret;
		while (true) {
			step_ret = sqlite3_step(statement);
			switch (step_ret) {
				case SQLITE_ROW:
					if (first) {
						print_header(statement);
						first = false;
					}
					print_row(statement);
					break;
				case SQLITE_DONE:
					goto loop_done;
				default:
					LOG_ERROR("error %u stepping: %s", step_ret, sqlite3_errmsg(database));
					goto loop_done;
			}
		}
loop_done:;

		sqlite3_finalize(statement);
	}

	assert(sqlite3_close(database) == SQLITE_OK, "closing database");
	assert(sqlite3_shutdown() == SQLITE_OK, "shutting down sqlite");

	uart_send_str("now idle, can be unplugged\r\n");
}
