INCLUDE_DIR := include
SRC_DIR := src
BUILD_DIR := target

KERNEL_SOURCES := \
	boot.s \
	devices/aht20.c \
	devices/lcd.c \
	devices/mcp23017.c \
	exception.c \
	exception.s \
	framebuffer.c \
	gpio.c \
	halt.c \
	i2c.c \
	init.c \
	mailbox.c \
	main.c \
	printf.c \
	pwm.c \
	sleep.c \
	string.c \
	timer.c \
	uart.c \
	mmu.c \
	emmc.c \
	log.c \
	malloc.c \
	sqlite-api.c \
	gpt.c \
	random.c \
	time.c \
	devices/ds3231.c \
	math.c \

ARMSTUB_SOURCES := armstub.s

CFLAGS_SHARED := -O2 -std=gnu2x -ffreestanding -nostdinc -mcpu=cortex-a72
CFLAGS := $(CFLAGS_SHARED) -iquote$(INCLUDE_DIR) -isystemsqlite -MMD -MP -include$(INCLUDE_DIR)/common.h -Wall -Wextra -Weverything -Wno-pre-c2x-compat -Wno-declaration-after-statement -Wno-gnu-empty-struct -Wno-c++-compat -Wno-gnu -Wno-c++98-compat -Wno-reserved-identifier -Wno-fixed-enum-extension -Wno-switch-enum -Wno-pedantic -g
CFLAGS_SQLITE := $(CFLAGS_SHARED) -DSQLITE_THREADSAFE=0 -DSQLITE_OS_OTHER=1 -DSQLITE_DQS=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_DEPRECATED -DSQLITE_USE_ALLOCA -DHAVE_MALLOC_USABLE_SIZE -DSQLITE_TEMP_STORE=3 -DSQLITE_ENABLE_API_ARMOR -DSQLITE_OMIT_LOCALTIME -DSQLITE_OMIT_JSON -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_INCRBLOB -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_OMIT_TCL_VARIABLE -DSQLITE_OMIT_UTF16 -DSQLITE_OMIT_WAL -Wno-everything

CC := clang --target=aarch64-unknown-none
OBJCOPY := llvm-objcopy
LD := ld.lld -m aarch64linux

.DEFAULT_GOAL := images

-include $(patsubst %,$(BUILD_DIR)/%.d,$(KERNEL_SOURCES))

$(BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	mkdir -p $(shell dirname $@)
	$(CC) -c $< -o $@

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.bin.o: %.bin
	mkdir -p $(shell dirname $@)
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(BUILD_DIR)/sqlite.o: sqlite
	$(CC) $(CFLAGS_SQLITE) $</sqlite3.c -I$< -c -o $@

$(BUILD_DIR)/rust.a: rust rust/**
	mkdir -p $(shell dirname $@)
	cd $<; cargo build --manifest-path Cargo.toml --release --target aarch64-unknown-none
	cp $(shell cargo metadata --manifest-path $</Cargo.toml --format-version 1 | jq --raw-output .target_directory)/aarch64-unknown-none/release/libpios_utils.a $@

$(BUILD_DIR)/kernel8.elf: linker.ld $(patsubst %,$(BUILD_DIR)/%.o,$(KERNEL_SOURCES)) $(BUILD_DIR)/rust.a $(BUILD_DIR)/sqlite.o
	mkdir -p $(shell dirname $@)
	$(LD) -T $^ -o $@

$(BUILD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.elf
	mkdir -p $(shell dirname $@)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/armstub.elf: $(patsubst %,$(BUILD_DIR)/%.o,$(ARMSTUB_SOURCES))
	mkdir -p $(shell dirname $@)
	$(LD) --section-start=.text=0 $^ -o $@

$(BUILD_DIR)/armstub.img: $(BUILD_DIR)/armstub.elf
	mkdir -p $(shell dirname $@)
	$(OBJCOPY) -O binary $< $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: images
images: $(BUILD_DIR)/kernel8.img $(BUILD_DIR)/armstub.img

$(SD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.img
	cp $< $@
$(SD_DIR)/config.txt: config.txt
	cp $< $@
$(SD_DIR)/armstub.img: $(BUILD_DIR)/armstub.img
	cp $< $@

.PHONY: install
install: $(SD_DIR)/kernel8.img $(SD_DIR)/config.txt $(SD_DIR)/armstub.img
