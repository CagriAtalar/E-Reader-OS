# Makefile — cagOS readerOS build system
# All commands run via WSL (Linux toolchain)

.PHONY: all run iso clean m0

# Tools
CC      = gcc
LD      = ld
NASM    = nasm
OBJCOPY = objcopy
PYTHON  = python3
QEMU    = qemu-system-i386
XORRISO = xorriso

# Directories
BUILD   = build
BOOT    = boot
KERNEL  = kernel
TEXT    = text
TOOLS   = tools
TEXTS   = texts

# Compiler flags
CFLAGS  = -m32 -ffreestanding -nostdlib -nostdinc -fno-stack-protector \
          -fno-builtin -fno-pie -Wall -Wextra -O2 -I$(KERNEL) -Itext
LDFLAGS = -m elf_i386 -T $(KERNEL)/linker.ld --gc-sections

# Source files
KERNEL_SRCS = $(KERNEL)/main.c $(KERNEL)/serial.c $(KERNEL)/vga.c \
              $(KERNEL)/ps2.c $(KERNEL)/libc_min.c
READER_SRCS = $(TEXT)/reader.c
MANIFEST_C  = $(BUILD)/manifest.c

ALL_SRCS = $(KERNEL_SRCS) $(READER_SRCS) $(MANIFEST_C)
ALL_OBJS = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(ALL_SRCS)))

# ================================================================
# M0: Hello boot sector test
# ================================================================
m0: $(BUILD)/hello.img
	$(QEMU) -hda $(BUILD)/hello.img -nographic -no-reboot

$(BUILD)/hello.img: $(BOOT)/hello.asm | $(BUILD)
	$(NASM) -f bin -o $@ $<

# ================================================================
# Main build: all milestones
# ================================================================
all: $(BUILD)/disk.img

# Step 1: Generate manifest from texts/
$(BUILD)/manifest.c $(BUILD)/text_blob.bin: $(wildcard $(TEXTS)/*.txt) $(TOOLS)/gen_manifest.py | $(BUILD)
	$(PYTHON) $(TOOLS)/gen_manifest.py $(TEXTS) $(BUILD)/manifest.c $(BUILD)/text_blob.bin

# Step 2: Compile all C sources
$(BUILD)/main.o: $(KERNEL)/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/serial.o: $(KERNEL)/serial.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/vga.o: $(KERNEL)/vga.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ps2.o: $(KERNEL)/ps2.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/libc_min.o: $(KERNEL)/libc_min.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/reader.o: $(TEXT)/reader.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/manifest.o: $(BUILD)/manifest.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(KERNEL) -c -o $@ $<

# Step 3: Create __text_blob_start symbol
# The blob is appended after the kernel binary, so its address is
# 0x100000 + kernel_size (aligned to 512-byte sector boundary).
# We compute this after linking the kernel ELF.

# Link kernel ELF (first pass — with dummy blob symbol to measure size)
$(BUILD)/kernel_noblob.elf: $(BUILD)/main.o $(BUILD)/serial.o $(BUILD)/vga.o \
                            $(BUILD)/ps2.o $(BUILD)/libc_min.o $(BUILD)/reader.o \
                            $(BUILD)/manifest.o $(KERNEL)/linker.ld
	@# Create a dummy symbol for first pass (address doesn't matter, just need it to link)
	@echo "SECTIONS { .dummy 0 : { __text_blob_start = 0xDEAD0000; } }" > $(BUILD)/dummy_blob.ld
	@echo "" | $(CC) -m32 -x c -c -o $(BUILD)/dummy_blob.o -
	$(LD) -m elf_i386 -r -T $(BUILD)/dummy_blob.ld -o $(BUILD)/dummy_blob_sym.o $(BUILD)/dummy_blob.o
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/main.o $(BUILD)/serial.o $(BUILD)/vga.o \
	    $(BUILD)/ps2.o $(BUILD)/libc_min.o $(BUILD)/reader.o $(BUILD)/manifest.o \
	    $(BUILD)/dummy_blob_sym.o

# Convert to flat binary to get exact size
$(BUILD)/kernel_noblob.bin: $(BUILD)/kernel_noblob.elf
	$(OBJCOPY) -O binary $< $@

# Generate the blob address symbol and re-link
$(BUILD)/blob_addr.o: $(BUILD)/kernel_noblob.bin $(BUILD)/text_blob.bin
	@# Kernel size, aligned to 512-byte boundary
	$(eval KSIZE := $(shell wc -c < $(BUILD)/kernel_noblob.bin))
	$(eval KSIZE_ALIGNED := $(shell echo $$(( ($(KSIZE) + 511) / 512 * 512 ))))
	$(eval BLOB_ADDR := $(shell printf "0x%x" $$((0x100000 + $(KSIZE_ALIGNED)))))
	@echo "Kernel size: $(KSIZE) -> aligned: $(KSIZE_ALIGNED), blob at: $(BLOB_ADDR)"
	@echo "SECTIONS { .blob_sym 0 : { __text_blob_start = $(BLOB_ADDR); } }" > $(BUILD)/blob_addr.ld
	@# Create a minimal object with just the symbol
	echo "" | $(CC) -m32 -x c -c -o $(BUILD)/blob_addr_dummy.o -
	$(LD) -m elf_i386 -r -T $(BUILD)/blob_addr.ld -o $@ $(BUILD)/blob_addr_dummy.o

# Final kernel ELF with blob symbol
$(BUILD)/kernel.elf: $(BUILD)/main.o $(BUILD)/serial.o $(BUILD)/vga.o \
                     $(BUILD)/ps2.o $(BUILD)/libc_min.o $(BUILD)/reader.o \
                     $(BUILD)/manifest.o $(BUILD)/blob_addr.o $(KERNEL)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/main.o $(BUILD)/serial.o $(BUILD)/vga.o \
	    $(BUILD)/ps2.o $(BUILD)/libc_min.o $(BUILD)/reader.o $(BUILD)/manifest.o \
	    $(BUILD)/blob_addr.o

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# Step 4: Assemble boot sector with total sector count patched in
$(BUILD)/boot.bin: $(BOOT)/boot.asm $(BUILD)/kernel.bin $(BUILD)/text_blob.bin
	@# Calculate total sectors: (kernel_aligned + blob_size + 511) / 512
	$(eval KBIN_SIZE := $(shell wc -c < $(BUILD)/kernel.bin))
	$(eval KBIN_ALIGNED := $(shell echo $$(( ($(KBIN_SIZE) + 511) / 512 * 512 ))))
	$(eval BLOB_SIZE := $(shell wc -c < $(BUILD)/text_blob.bin))
	$(eval TOTAL_BYTES := $(shell echo $$(($(KBIN_ALIGNED) + $(BLOB_SIZE)))))
	$(eval TOTAL_SECTORS := $(shell echo $$(( ($(TOTAL_BYTES) + 511) / 512 ))))
	@echo "Kernel: $(KBIN_SIZE)B, Blob: $(BLOB_SIZE)B, Total: $(TOTAL_SECTORS) sectors"
	$(NASM) -f bin -DTOTAL_SECTORS=$(TOTAL_SECTORS) -o $@ $<

# Step 5: Assemble disk image
# Layout: [boot.bin 512B] [kernel.bin padded] [text_blob.bin]
$(BUILD)/disk.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin $(BUILD)/text_blob.bin
	@KBIN_SIZE=$$(wc -c < $(BUILD)/kernel.bin); \
	KBIN_ALIGNED=$$(( (KBIN_SIZE + 511) / 512 * 512 )); \
	BLOB_SIZE=$$(wc -c < $(BUILD)/text_blob.bin); \
	echo "Assembling disk: kernel=$${KBIN_SIZE}B (aligned=$${KBIN_ALIGNED}B), blob=$${BLOB_SIZE}B"; \
	cp $(BUILD)/boot.bin $@; \
	dd if=$(BUILD)/kernel.bin of=$@ bs=1 seek=512 conv=notrunc 2>/dev/null; \
	truncate -s $$((512 + KBIN_ALIGNED)) $@; \
	cat $(BUILD)/text_blob.bin >> $@; \
	FINAL_SIZE=$$(wc -c < $@); \
	PADDED=$$(( ((FINAL_SIZE + 511) / 512) * 512 )); \
	truncate -s $$PADDED $@; \
	echo "Disk image: $@ ($$PADDED bytes, $$((PADDED / 512)) sectors)"

# ================================================================
# Run in QEMU
# ================================================================
run: $(BUILD)/disk.img
	$(QEMU) -hda $(BUILD)/disk.img -serial stdio -no-reboot -m 32

# ================================================================
# ISO for VirtualBox
# ================================================================
iso: $(BUILD)/disk.img
	$(eval IMG_SECTORS := $(shell echo $$(( $$(wc -c < $(BUILD)/disk.img) / 512 ))))
	$(XORRISO) -as mkisofs \
	    -b disk.img \
	    -no-emul-boot \
	    -boot-load-size $(IMG_SECTORS) \
	    -o $(BUILD)/disk.iso \
	    $(BUILD)/disk.img
	@echo "ISO created: $(BUILD)/disk.iso"

# ================================================================
# Utilities
# ================================================================
$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

help:
	@echo "Targets:"
	@echo "  m0      - Build and run 'Hello' boot sector in QEMU"
	@echo "  all     - Build complete disk image"
	@echo "  run     - Build and run in QEMU"
	@echo "  iso     - Build El Torito ISO for VirtualBox"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help"
