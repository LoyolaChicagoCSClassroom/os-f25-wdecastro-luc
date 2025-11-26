UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),aarch64)
PREFIX:=i686-linux-gnu-
BOOTIMG:=/usr/local/grub/lib/grub/i386-pc/boot.img
GRUBLOC:=/usr/local/grub/bin/
else
PREFIX:=
BOOTIMG:=/usr/lib/grub/i386-pc/boot.img
GRUBLOC :=
endif
CC := $(PREFIX)gcc
LD := $(PREFIX)ld
OBJDUMP := $(PREFIX)objdump
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size
CONFIGS := -DCONFIG_HEAP_SIZE=4096
CFLAGS := -ffreestanding -mgeneral-regs-only -mno-mmx -m32 -march=i386 -fno-pie -fno-stack-protector -g3 -Wall
ODIR = obj
SDIR = src
OBJS = \
        kernel_main.o \
        vga_output.o \
        page.o

# Make sure to keep a blank line here after OBJS list
OBJ = $(patsubst %,$(ODIR)/%,$(OBJS))

all: bin rootfs.img

obj:
	mkdir -p obj

# Rule for assembling boot.asm
obj/boot.o: src/multiboot.asm | obj
	nasm -f elf32 $< -o $@

# Rules for C files
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# Build kernel - boot.o must be first
bin: obj/boot.o $(OBJ)
	$(LD) -melf_i386 obj/boot.o obj/kernel_main.o obj/vga_output.o obj/page.o -Tkernel.ld -o kernel
	$(SIZE) kernel

rootfs.img: bin
	dd if=/dev/zero of=rootfs.img bs=1M count=32
	$(GRUBLOC)grub-mkimage -p "(hd0,msdos1)/boot" -o grub.img -O i386-pc normal biosdisk multiboot multiboot2 configfile fat exfat part_msdos
	dd if=$(BOOTIMG) of=rootfs.img conv=notrunc
	dd if=grub.img of=rootfs.img conv=notrunc bs=512 seek=1
	echo 'start=2048, type=83, bootable' | sfdisk rootfs.img
	mkfs.vfat --offset 2048 -F16 rootfs.img
	mcopy -i rootfs.img@@1M kernel ::/
	mmd -i rootfs.img@@1M boot 
	mcopy -i rootfs.img@@1M grub.cfg ::/boot
	@echo " -- BUILD COMPLETED SUCCESSFULLY --"

run:
	qemu-system-i386 -hda rootfs.img

debug:
	./launch_qemu.sh

clean:
	rm -f grub.img kernel rootfs.img obj/*
