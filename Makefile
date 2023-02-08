INCLUDE_DIR := include
SRC_DIR := src

SOURCES := boot.s kernel.c mailbox.c gpio.c framebuffer.c sleep.c pwm.c clock.c

CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles -iquote$(INCLUDE_DIR) -include$(INCLUDE_DIR)/common.h
TOOLCHAIN = aarch64-elf-
BUILD_DIR := target

.DEFAULT_GOAL := $(BUILD_DIR)/kernel8.img

$(BUILD_DIR)/:
	mkdir -p $@

$(BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s | $(BUILD_DIR)/
	$(TOOLCHAIN)gcc $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/
	$(TOOLCHAIN)gcc $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.bin.o: %.bin | $(BUILD_DIR)/
	$(TOOLCHAIN)objcopy -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(BUILD_DIR)/kernel8.elf: linker.ld $(patsubst %,$(BUILD_DIR)/%.o,$(SOURCES)) | $(BUILD_DIR)/
	$(TOOLCHAIN)ld -nostdlib -T $^ -o $@

$(BUILD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.elf | $(BUILD_DIR)/
	$(TOOLCHAIN)objcopy -O binary $< $@

.PHONY: clean install
clean:
	rm -rf $(BUILD_DIR)

$(SD_DIR)/kernel8.img: $(BUILD_DIR)/kernel8.img
	cp $< $@
$(SD_DIR)/config.txt: config.txt
	cp $< $@

install: $(SD_DIR)/kernel8.img $(SD_DIR)/config.txt
