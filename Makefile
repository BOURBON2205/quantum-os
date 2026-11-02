CC      = gcc
AS      = gcc
LD      = ld
CFLAGS  = -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
          -mno-sse -mno-mmx -mno-80387 -O1 -Wall -Wextra -Isrc
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z noexecstack

SRC_C = $(wildcard src/*.c)
SRC_S = $(wildcard src/*.S)
OBJ   = $(patsubst src/%.c,build/%.o,$(SRC_C)) $(patsubst src/%.S,build/%.o,$(SRC_S))
KERNEL = build/quantum.elf

all: $(KERNEL)

build/%.o: src/%.c src/kernel.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: src/%.S | build
	$(AS) -m32 -c $< -o $@
$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@
build:
	mkdir -p build

# QEMU memuat kernel Multiboot langsung; input dari jendela QEMU (keyboard) ATAU terminal (serial)
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 64M -serial stdio
# tanpa jendela grafis, semua lewat terminal
run-serial: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 64M -nographic
# bootloader GRUB sungguhan (butuh grub-pc-bin, xorriso, mtools)
iso: $(KERNEL)
	mkdir -p build/iso/boot/grub
	cp $(KERNEL) build/iso/boot/quantum.elf
	printf 'set timeout=0\nset default=0\nmenuentry "QuantumOS" {\n  multiboot /boot/quantum.elf\n  boot\n}\n' > build/iso/boot/grub/grub.cfg
	grub-mkrescue -o build/quantum.iso build/iso
run-iso: iso
	qemu-system-i386 -cdrom build/quantum.iso -m 64M -serial stdio
# debug dengan gdb: qemu menunggu di port 1234
debug: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 64M -serial stdio -s -S
clean:
	rm -rf build
.PHONY: all run run-serial iso run-iso debug clean
