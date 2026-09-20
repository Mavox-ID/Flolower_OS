CC = gcc

CORE_SRC = src/window.c src/desktop.c src/taskbar.c src/startmenu.c \
           src/notify.c src/text.c src/apps.c src/app_calculator.c \
           src/app_paint.c src/app_calendar.c src/app_taskmanager.c \
           src/app_timer.c src/app_settings.c src/app_smilegame.c src/theme_state.c \
           src/kblayout.c src/main.c

FS_APP_SRC = src/app_terminal.c src/app_files.c src/app_texteditor.c src/app_browser.c src/app_notes.c src/app_trash.c src/persist.c

HOSTED_CFLAGS = -Wall -Wextra -std=c11 -O2 -DPLATFORM_HOSTED -DHAVE_FS $(shell pkg-config --cflags sdl2)
HOSTED_LDFLAGS = $(shell pkg-config --libs sdl2)
HOSTED_SRC = $(CORE_SRC) $(FS_APP_SRC) src/platform_sdl.c
HOSTED_OBJ = $(HOSTED_SRC:.c=.hosted.o)
HOSTED_BIN = flolower_shell

hosted: $(HOSTED_BIN)

$(HOSTED_BIN): $(HOSTED_OBJ)
	$(CC) $(HOSTED_OBJ) -o $(HOSTED_BIN) $(HOSTED_LDFLAGS)

%.hosted.o: %.c
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

BM_CFLAGS = -Wall -Wextra -std=c11 -O2 -m32 -ffreestanding -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DHAVE_FS -I src
BM_SRC = $(CORE_SRC) $(FS_APP_SRC) src/platform_baremetal.c
BM_OBJ = $(BM_SRC:.c=.bm.o)
BM_OUT = flolower_shell_baremetal.o

baremetal: $(BM_OUT)

$(BM_OUT): $(BM_OBJ)
	ld -m elf_i386 -r $(BM_OBJ) -o $(BM_OUT)

%.bm.o: %.c
	$(CC) $(BM_CFLAGS) -c $< -o $@

all: hosted

clean:
	rm -f src/*.hosted.o src/*.bm.o $(HOSTED_BIN) $(BM_OUT)
	rm -f kernel/*.o flolower.img boot.bin kernel.bin repo.bin

NASM = nasm
KCC  = gcc
KCFLAGS = -Wall -Wextra -std=c11 -O2 -m32 -ffreestanding -fno-builtin -fno-stack-protector \
          -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
          -DREPO_START_LBA=$(REPO_START_LBA) \
          -I kernel -I src

KERNEL_C_SRC = kernel/serial.c kernel/pic.c kernel/idt.c kernel/isr.c kernel/pit.c \
               kernel/ps2.c kernel/vesa.c kernel/libc_lite.c kernel/heap.c \
               kernel/ramfs.c kernel/fs_shim.c kernel/ata.c kernel/repo.c kernel/process.c \
               kernel/reboot.c kernel/speaker.c kernel/acpi.c kernel/aml.c kernel/ec.c \
               kernel/battery.c kernel/power.c kernel/kernel_main.c
KERNEL_ASM_SRC = kernel/entry.asm kernel/idt_stubs.asm kernel/context_switch.asm
KERNEL_OBJ = $(KERNEL_C_SRC:.c=.o) $(KERNEL_ASM_SRC:.asm=.o)

kernel/%.o: kernel/%.c
	$(KCC) $(KCFLAGS) -c $< -o $@

kernel/%.o: kernel/%.asm
	$(NASM) -f elf32 $< -o $@

boot.bin: kernel/boot.asm
	$(NASM) -f bin kernel/boot.asm -o boot.bin

kernel.bin: $(KERNEL_OBJ) $(BM_OBJ)
	ld -m elf_i386 -T kernel/linker.ld -o kernel.elf $(KERNEL_OBJ) $(BM_OBJ) \
	   $(shell $(KCC) -m32 -print-libgcc-file-name)
	objcopy -O binary kernel.elf kernel.bin
	@size=$$(stat -c%s kernel.bin); \
	budget=$$(( 400 * 512 )); \
	if [ $$size -gt $$budget ]; then \
	  echo "ERROR: kernel.bin is $$size bytes, exceeds boot.asm's KERNEL_SECTORS budget ($$budget bytes)."; \
	  echo "Bump KERNEL_SECTORS in kernel/boot.asm or this will silently truncate at boot."; \
	  exit 1; \
	fi; \
	echo "kernel.bin: $$size bytes ($$(( ($$size + 511) / 512 )) sectors, budget 400)"

REPO_START_LBA = 2000

repo.bin: tools/build_repo.py $(wildcard repo/*.txt)
	python3 tools/build_repo.py

flolower.img: boot.bin kernel.bin repo.bin
	cp boot.bin flolower.img
	dd if=/dev/zero bs=512 count=16383 >> flolower.img 2>/dev/null
	dd if=kernel.bin of=flolower.img bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=repo.bin of=flolower.img bs=512 seek=$(REPO_START_LBA) conv=notrunc 2>/dev/null

image: flolower.img

run: image
	qemu-system-i386 -m 128M -drive file=flolower.img,format=raw,if=ide -serial stdio

.PHONY: all hosted baremetal clean image run
