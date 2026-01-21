# $@=target file
# $<=first dependency
# $^=all dependencies

# 搜尋所有的 C 原始碼檔案
C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c)
# 搜尋所有的標頭檔
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h)

# 定義目標物件檔案列表
OBJS = ${C_SOURCES:.c=.o} cpu/interrupt.o

# 指定編譯器與連結器路徑
CC= /usr/local/i386elfgcc/bin/i386-elf-gcc
LD= /usr/local/i386elfgcc/bin/i386-elf-ld
GDB = /usr/bin/gdb

# 編譯選項 (-g 為除錯資訊，-ffreestanding 為獨立編譯，-Wall -Wextra 為警告，-fno-exceptions 禁用異常處理機制，-m32 為32位元模式, -fstack-protector-strong 啟用完全堆疊保護)
CFLAGS = -g -ffreestanding -Wall -Wextra -fno-exceptions -m32 -fstack-protector-strong


# 組合開機磁區與核心，製作作業系統映像檔
os-image.bin: boot/bootsect.bin kernel.bin
	cat $^ > $@


# 連結核心物件檔案，生成純二進制核心
kernel.bin: boot/kernel_entry.o ${OBJS}
	${LD} -o $@ -T linker.ld $^ --oformat binary


# 生成帶有符號表的 ELF 核心檔案，用於除錯
kernel.elf: boot/kernel_entry.o ${OBJS}
	${LD} -o $@ -T linker.ld $^ 


# 執行 QEMU 模擬器 (加入音訊支援, 設定RTC同步)
run: os-image.bin
	qemu-system-i386 -fda os-image.bin -audiodev pa,id=audio0 -machine pcspk-audiodev=audio0 -rtc base=utc


# 啟動 QEMU 並連接 GDB 進行除錯
debug : os-image.bin kernel.elf
	qemu-system-i386 -s -fda os-image.bin -rtc base=utc &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

# 編譯 C 程式碼為物件檔案
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS}  -c $< -o $@

# 編譯組合語言為 ELF 格式物件檔案
%.o : %.asm
	nasm $< -f elf -o $@

# 編譯組合語言為純二進制檔案
%.bin: %.asm
	nasm $< -f bin -I boot/ -o $@


# 清除所有產生的檔案
clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o
