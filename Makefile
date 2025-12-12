# $@=target file
# $<=first dependency
# $^=all dependencies

all: run

kernel.bin: kernel.o kernel_entry.o
	i386-elf-ld -o $@ -Ttext 0x1000 $^ --oformat binary

kernel_entry.o : kernel_entry.asm
	nasm $< -f elf -o $@

kernel.o : kernel.c
	i386-elf-gcc -ffreestanding -c $< -o $@


# rule of disassemble the kernel， usefule to debug
kernel.dis : kernel.bin
	ndisasm -b 32 $< > $@

bootsect.bin: bootsect.asm
	nasm $< -f bin -o $@


os-image.bin: bootsect.bin kernel.bin
	cat $^ > $@

run:os-image.bin
	qemu-system-i386 -fda $<


clean:
	rm -rf *.o *.bin