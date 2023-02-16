INCLUDE_DIR := include
SRC_DIR := src
BUILD_DIR := target

SOURCES := boot.s main.c mailbox.c gpio.c framebuffer.c sleep.c pwm.c clock.c uart.c string.c i2c.c init.c devices/mcp23017.c printf.c

CFLAGS := -Wall -Wextra -O2 -std=gnu2x -ffreestanding -nostdinc -iquote$(INCLUDE_DIR) -include$(INCLUDE_DIR)/common.h

CC := clang --target=aarch64-unknown-none
OBJCOPY := llvm-objcopy
LD := ld.lld -m aarch64linux

.DEFAULT_GOAL := $(BUILD_DIR)/kernel8.img

$(BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	mkdir -p $(shell dirname $@)
	$(CC) -c $< -o $@

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.bin.o: %.bin
	mkdir -p $(shell dirname $@)
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(BUILD_DIR)/kernel8.elf: linker.ld $(patsubst %,$(BUILD_DIR)/%.o,$(SOURCES))
	mkdir -p $(shell dirname $@)
	$(LD) -T $^ -o $@

$(BUILD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.elf | $(BUILD_DIR)/
	mkdir -p $(shell dirname $@)
	$(OBJCOPY) -O binary $< $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

$(SD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.img
	cp $< $@
$(SD_DIR)/config.txt: config.txt
	cp $< $@

.PHONY: install
install: $(SD_DIR)/kernel8.img $(SD_DIR)/config.txt
