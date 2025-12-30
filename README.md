# os

## bootsector

當電腦開啟時，bios不知道如何載入os，這是boot sector的任務，boot sector一定要放在標準位置，位置在第一個磁碟的磁區(cylinder 0, head 0, sector 0)，整個boot sector有512bytes大  

確保disk可以bootable，bios檢查boot sector的第511 bytes和512 bytes是0xAA55

下面是最簡單的boot sector

```c
e9 fd ff 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[ 29 more lines with sixteen zero-bytes each ]
00 00 00 00 00 00 00 00 00 00 00 00 00 00 55 aa
```

基本上都是0，在後面的16-bits value是0xAA55(要注意是little-endian,x86)
第一列中的3個bytes是代表無限jump迴圈

看boot_sect.asm代碼

編譯指令
```
 nasm -f bin boot_sect.asm -o boot_sect.bin
```

用qemu模擬
```c
qemu-system-x86_64  boot_sect.bin 
```


## bootsector-print

讓boot sector印出文字

改進我們的無限迴圈的boot sector可以印出東西顯示在螢幕上，將要發出interrupt信號做到這件事

把"Hello"的每一字元給放到register(al,lower part of ax)，還有0x0e放入到ah(the higher part of x)，最後使用0x10去發起中斷顯示字元在螢幕上

ah存入0x0e是傳送中斷訊號到螢幕，在tty mode讓al中的內容顯示到螢幕上

查看boot_sect_hello.asm

編譯指令
```c
nasm -f bin boot_sect_hello.asm -o boot_sect_hello.bin
```

執行
```c
qemu-system-x86_64  boot_sect_hello.bin 
```

## bootsector-memory

learn where the boot sector is stored

直接說bios一定都放在0x7c00位置上  
說法來源參考  
https://gist.github.com/letoh/2790559  
https://zhuanlan.zhihu.com/p/655209631  

將x顯在螢幕上，嘗試4種不同的方法實現

查看boot_sect_memory.asm

先將定義X作為資料，用label指定記憶體位置，不用知道實際的記憶體位置，也能存取到x
```c
the_secret:
    db "X"
```

嘗試用不同方式存取the_secret
1. mov al, the_secret
2. mov al, [the_secret]
3. mov al, the_secret + 0x7C00
4. mov al, 2d + 0x7C00，2d代表實際"X"的位置

編譯指令
```c
 nasm -f bin boot_sect_memory.asm -o boot_sect_memory.bin
```

執行
```c
qemu-system-x86_64  boot_sect_memory.bin
```

應該會看到字串像`1[2¢3X4X`

為甚麼X的地址位移量是0x2d呢?用xxd工具來檢查

使用 `xxd` 工具檢查編譯後的二進制文件：

```bash
xxd -l 0x30 boot_sect_memory.bin
```

輸出結果：
```
00000000: b40e b031 cd10 b02d cd10 b032 cd10 a02d  ...1...-...2...-
00000010: 00cd 10b0 33cd 10bb 2d00 81c3 007c 8a07  ....3...-....|..
00000020: cd10 b034 cd10 a02d 7ccd 10eb fe58 0000  ...4...-|....X..
```

可以看到 "X" 字元（0x58）出現在偏移量 **0x2d** 的位置。

使用 `nasm -l` 生成列表文件可以更清楚地看到：

```bash
nasm -f bin boot_sect_memory.asm -o boot_sect_memory.bin -l boot_sect_memory.lst
```

從列表文件中可以看到：
- `jmp $` 指令在 0x0000002B 位置（2 bytes: EBFE）
- `the_secret:` 標籤緊接在後面，位於 **0x0000002D** 位置
- `X` 字元（0x58）就存儲在這個位置

所以位移量 0x2d 是從 boot sector 開始（0x0000）到 `the_secret` 標籤的距離，也就是所有前面的指令代碼（包括 4 種打印方法的代碼和 `jmp $` 指令）的總長度。



## The global offset
現在，assemblers可以定義global offset作為每個記憶體的base address
加上org指令
```
[org 0x7c00]
```
重跑一次之前程式，之前的第二方法就可以正確執行，也會影響之前的指令


## bootsector-stack

運用`bp` register 保存stack的base address，還有`sp`保存stack的top address  

還有stack是由上而下的生長的，所以通常sp都是用減的來代表新top

這邊直接看boot_sect_stack.asm程式碼看stack運作

編譯指令
```c
nasm -f bin boot_sect_stack.asm -o boot_sect_stack.bin
```

執行
```c
qemu-system-x86_64  boot_sect_stack.bin 
```


## bootsector-functions-strings

這節要學習寫control structures，functional calling，full strings usage

作為在準備進入disk和kernel的前置概念

### Strings
定義string就像一長串的characters集合，後面在加一個null byte來當字串結尾
```c
mystring:
    db 'Hello,World',0
```

### Control structures
到目前的程式有使用'jmp $'當作無限迴圈  

assembler跳到之前的指令結果
範例:
```c
cmp ax,4 ; if ax=4
je ax_is_four ; do something when ax result equal to 4
jmp else ;else do another thing
jmp endif ; finally ,resume the normal flow

ax_is_four:
    .....
    jmp endif

else:
    .....
    jmp endif ;為了完整性這邊也跳到endif，其實`else:`執行完，就會往下執行

endif:

```

### Calling functions

calling a function is just a jump to a label 
呼叫函式就像跳到label一樣 

有兩個步驟傳入參數

1. the programmer knows theys share a specific register or memory address
2. write a bit more code and make function calls generic and without side effects

第一步，使用al(actually ax)作為傳入參數

```c
mov al,'X'
jmp print
endprint:

...

print:
    mov ah,0x0e ; tty mode
    int 0x10 ;assume the 'al' already has the character
    jmp endprint

```

可以看到這方法很容意變成雜亂的程式碼，現在的print函式只有返回endprint，如果是其他函式想要呼叫，將會浪費程式碼資源  

正確的解法是提供兩個改進點  

- 儲存返回位址  
- 儲存現在的register，這樣在函式中修改，不會修改到原本的register的數值  

為了可以儲存return address，cpu將會幫忙取代`jmp`去呼叫函式，使用`call`和`ret`  

為了儲存register data，也有stack特殊指令，`pusha`和`popa`，可以把全部register的數值自動保存和恢復  

###  include external files

依照功能分開程式碼，並且印入到main檔，可以增加可讀性

```c
%include "file.asm"
```

編譯指令
```c
nasm -f bin boot_sect_main.asm  -o boot_sect_main.bin
```
不用把boot_sect_print.asm給一起編譯進去

執行
```
qemu-system-x86_64 boot_sect_main.bin 
```

### print hex values

下個目標是要讀取磁碟，所以需要一些方法確保讀取到正確資料，查看`boot_sect_print_hex.asm`

## bootsector-segmentation

learn how to address memory with 16-bit real mode segmentation

did segmentation with [org] before

Segmentation means that you can specify an offset to all the data you refer to

use `cs`,`ds`,`ss`,`es` for code,data,stack ,extra

warning:these register are  implicitly used by the CPU ，then all your memory access will be offset by `ds`

compute real address formula `segment<<4+address`，for example,`ds` is `0x4d` , then `[0x20]` actully refers to `0x4d+0x20=0x4f0`

note:can't use `mov` literals to those registers,have to use a gernel purpose register before

編譯指令
```c
nasm -f bin boot_sect_segmentation.asm  -o boot_sect_segmentation.bin
```

執行指令
```c
qemu-system-x86_64 boot_sect_segmentation.bin
```

## bootsector-disk

let the bootsector load data from disk in order to boot the kernel

OS can't fit inside the bootsector 512 bytes,need to read data from  a disk to run kernel

Don't need deal with the disk spinning platters on and off

can just call bios routines,To do that, set `al` to `0x02` (other reg with required cylinder ,head and sector ) and raise int 0x13

more int 13h detail https://stanislavs.org/helppc/int_13-2.html  

this time will use the carry bit,which is an extra bit present on each register stores when an operation has overflow status 

```c
mov ax, 0xFFFF
add ax, 1 ; ax = 0x0000 and carry = 1
```

carry bit isn't accessed but used as a control bit by other operators ,like `jc` (jump if the carry bit is set)

bios set `al` the number of sectors read,always compare it to the expected number

編譯指令
```c
nasm -f bin boot_sect_disk_main.asm  -o boot_sect_disk_main.bin
```

執行
```c
qemu-system-x86_64 boot_sect_disk_main.bin  //自動是視為軟盤
qemu-system-x86_64  -fda boot_sect_disk_main.bin //直接用floopy disk開啟
```


## 32-bit print

print on the screen when on 32-bit protected mode

32-bit mode can use 32 bit register and memory addressing，protected memory ,virtual memory。but will lose bios interrupts and need to code the GDT

write print string loop which work in 32-bit mode，don't need bios interrupt  

Directly operate VGA video memory install of calling `int 0x10` 

the VGA memory at address `0xb8000` and has text mode to avoid operate directly pixels  

the formula for access a specific character on the 80x25 gird:  
`0xb8000`+2*(row*80+col)

Every character use 2 bytes(one for ascii,the other is for color)

## 32bit-gdt

The segment operation way is left shifted to address an extrax level of indirection

In 32-bit mode ，segmenation work differently，the offset is an index to a segment descriptor `(SD)` in the GDT。this descriptor define the base address(32-bits),the size(20-bits) and some flags  

like readonly，permissions,etc

easy way to program the GDT is to define two segments,one for code and another for data,these can overlap which means there is no memory protection,but it's enough to boot

the first GDT entry must be 0x00 to make sure that the programer didn't make mistake managing address

The cpu can't  directly load the GDT address，it requires a meta structure called the `GDT descriptor` with the size(16b) and address (32b)of our actual GDT. it's loaded with the `lgdt` operation.

note:refernce os-dev.pdf check segement flags

## 32bit-enter

Enter 32-bit protected mode and test our code from previous code

To jump into 32-bit mode steps:
1. disable interrupts
2. laod the GDT
3. set a bit on the cpu control register `cr0`
4. Flush the CPU pipeline by issuing a carefully crafted far jump
5. Update all the segment registers
6. Update the stack
7. Call to a well-known label which contains the first usefull code in 32 bits

build the prcoess on the file `32bit-switch.asm`

after entering 32-bit mode,will call `BEGIN_PM` which is the entry point for actual useful code，can watch `32bit-main.asm`。

compile instruction
```c
nasm -f bin 32bit-main.asm  -o 32bit-main.bin
```

usage
```c
qemu-system-x86_64 32bit-main.bin
```


## kernel-crosscompiler

create a development environment to build your knerel

need a cross-compiler once jump to  developing in a higher language，it's C

First install the required packages 
- gmp
- mpfr
- libmpc
- gcc

will need `gcc` to build corss-compiled

```c
export CC=/usr/local/bin/gcc
export LD=/usr/local/bin/gcc
```

need to build binutils and a cross-compiled gcc,will put them into /user/local/i386elfgcc

```c
export PREFIX="/usr/local/i386elfgcc"
export TARGET=i386-elf
export PATH="$PREFIX/bin:$PATH"
```

### binutils
recommend copy line by line，execute with root 
```c
mkdir /tmp/src
cd /tmp/src
curl -O http://ftp.gnu.org/gnu/binutils/binutils-2.24.tar.gz # If the link 404's, look for a more recent version
tar xf binutils-2.24.tar.gz
mkdir binutils-build
cd binutils-build
../binutils-2.24/configure --target=$TARGET --enable-interwork --enable-multilib --disable-nls --disable-werror --prefix=$PREFIX 2>&1 | tee configure.log
```

### gcc
the same，use root execute these command
```c
cd /tmp/src
curl -O https://ftp.gnu.org/gnu/gcc/gcc-4.9.1/gcc-4.9.1.tar.bz2
tar xf gcc-4.9.1.tar.bz2
mkdir gcc-build
cd gcc-build
../gcc-4.9.1/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --disable-libssp --enable-languages=c --without-headers
make all-gcc 
make all-target-libgcc 
make install-gcc 
make install-target-libgcc 
```

should have all the GNU binutils and the compiler at `/usr/local/i386elfgcc/bin`

## kernel-c

learn to write the low-level code as we did with assembler, but in C

### Compile

see how the C compiler our code and compare it to the machine code generated with the assembler

start writing a simple program which contains a function `function.c`

To compile system-independent code,need the flag `-ffreestanding`
`ffreestanding`:very important,let compiler know it build kernel not user space program。need implement own `memset` ,`memcpy`,`memcmp` and `memove` function

compile instruction
```c
i386-elf-gcc -ffreestanding -c function.c -o function.o
```

examine the machine code generated by the compiler
```c
i386-elf-objdump -d function.o
```

## Link
To make a binary file,use the linker,important part of is to learn how high level languages call function labels。

which is the offset where our function will be placed in memory?

We don't actually know.for example,we'll place the offse at `0x0` and use the `binary` format which generates machine code without any labels and metadata

```c
i386-elf-ld -o function.bin -Ttext 0x0 --oformat binary function.o
```
maybe have warning appear when linking, ignore it

examine both `binary` files `function.o` and `function.bin` using `xxd`

can seen the `.bin` file is machine code

`.o` file has a lot of debugging information and labels..

## Decompile
examine the machine code
```c
ndisasm -b 32 function.bin
```

## more program
small programs,which feature
1. local variables `localvars.c`
2. function calls `functioncalls.c`
3. pointers `pointers.c`

Then compile and disassemble them，examine the resulting mahcine code

### compile program
```c
i386-elf-gcc -ffreestanding -c localvars.c -o localvars.o
i386-elf-gcc -ffreestanding -c functioncalls.c -o functioncalls.o
i386-elf-gcc -ffreestanding -c pointers.c -o pointers.o
```

exmine with `i386-elf-objdump`
```c
i386-elf-objdump -d localvars.o
i386-elf-objdump -d functioncalls.o
i386-elf-objdump -d pointers.o
```

### link program
```c
i386-elf-ld -o localvars.bin -Ttext 0x0 --oformat binary localvars.o
i386-elf-ld -o functioncalls.bin -Ttext 0x0 --oformat binary functioncalls.o
i386-elf-ld -o pointers.bin -Ttext 0x0 --oformat binary pointers.o
```
examine with xxd

```c
xxd localvars.o
xxd localvars.bin
xxd functioncalls.o
xxd functioncalls.bin
xxd pointers.o
xxd pointers.bin
```

### decompile program
```c
ndisasm -b 32 localvars.bin
ndisasm -b 32 functioncalls.bin
ndisasm -b 32 pointers.bin
```

why does the disassemblement of `pointers.c` not resemble what you would expect?
當你執行 ndisasm -b 32 pointers.bin 時，ndisasm 會傻傻地把整個檔案的所有位元組都當作 CPU 指令來翻譯。

編譯後的結構大致如下：
程式碼區段 (.text)：包含 mov 指令，用來把字串的「記憶體位址」存到變數裡。
資料區段 (.rodata)：緊接著程式碼之後，存放著實際的 "Hello" 字串（0x48, 0x65...）。
問題在於： ndisasm 讀到最後面的 "Hello" 時，它試圖把這些 ASCII 字元（0x48, 0x65...）解釋成組合語言指令，結果就變成了一堆奇怪、無意義的指令

Where is the ASCII `0x48656c6c6f` for "Hello"?

```c
00000000  55                push ebp          ; 儲存 stack base
00000001  89E5              mov ebp,esp       ; 設定新的 stack frame
00000003  83EC10            sub esp,byte +0x10 ; 預留 16 bytes 的局部變數空間
00000006  C745FC0F000000    mov dword [ebp-0x4],0xf  ; 關鍵點！
0000000D  C9                leave             ; 清除 stack frame
0000000E  C3                ret               ; 返回
```
關鍵點：注意第 54 行 mov dword [ebp-0x4],0xf。
這是把數值 0x0f 存入局部變數（也就是指標變數 string）。
0x0f 指向哪裡？指向檔案偏移量為 15 (十進位) 的位置。

緊接著 ret (0x0E) 之後，就是偏移量 0x0F。這裡正是編譯器放置字串常數 "Hello" 的地方。
但因為 ndisasm 只是盲目地翻譯，它把 "Hello" 的 ASCII 碼翻譯成了指令：
0x0F (offset): 48 -> ASCII 'H'。反組譯成 dec eax。
0x10 (offset): 65 -> ASCII 'e'。反組譯成 gs (前綴)。
0x11 (offset): 6C -> ASCII 'l'。反組譯成 insb。
0x12 (offset): 6C -> ASCII 'l'。反組譯成 insb。
0x13 (offset): 6F -> ASCII 'o'。反組譯成 outsd。
後面跟著的 00 (第 61 行 add [eax],al) 就是 C 語言字串結尾的 Null Terminator (\0)。


反組譯器很盡責地把「Hello」這幾個字翻譯成了「將暫存器減一」、「輸入字串」等毫無邏輯的 CPU 指令。這證明了馮·諾伊曼架構 (Von Neumann architecture) 的一個核心特性：記憶體中的資料和指令沒有區別，全看 CPU（或反組譯器）如何解讀它。


## kernel-barebones

create a simple kernel and a bootsector capable of booting it

### kernel
this time implement C kernel just print an `X`on the top left  corner of the screen

notice the dummy function that does nothing。That function will force us to create a kernel entry routinue which does not point to byte 0x0 in our kernel，but to an actual label which we know that launches it，the case is `main()`

compile instruction
```c
/usr/local/i386elfgcc/bin/i386-elf-gcc -ffreestanding -c kernel.c -o kernel.o
```

the routine code is `kernel_entry.asm`。will learn how to use `[extern]`  declarations in assebly。to compile this file in stread of generate a binary，will generate an `elf` format which will be linked with `kernel.o`

compile instruction
```c
nasm kernel_entry.asm -f elf -o kernel_entry.o
```

### the linker

linker is usefull tool

to link both object files into a single binary kernel and resolve label references

the kernel will be placed  not at `0x0` in memory，but at `0x1000`.the bootsector need to know this address too

compile instruction
```c
/usr/local/i386elfgcc/bin/i386-elf-ld -o kernel.bin -Ttext 0x1000 kernel_entry.o kernel.o --oformat binary
```

### the bootsector
`bootsector.asm` and examine the code

compile instruction
```c
nasm bootsect.asm -f bin -o bootsect.bin
```


### putting it all together
have tow separate files for the bootsector and the kernel

use link into a single file

concatenate instruction
```c
cat bootsect.bin kernel.bin > os-image.bin
```

### run os-image.bin

if have a disk laod errors that you may need to add qemu floppy parameters(floppy = `0x0`, hdd = `0x80`)
```c
qemu-system-i386 -fda os-image.bin  //from floopy 
```

can't use `qemu-system-i386 -hda os-image.bin` to start os from hard disk，because the os-image.bin is too small


will see four messages:
- "Started in 16-bit Real Mode"
- "Loading kernel into memory"
- (Top left) "Landed in 32-bit Protected Mode"
- (Top left, overwriting previous message) "X"


## check_point

learn how to debug the kernel with gdb

have already run own kernel，but it very little，just print 'X'

install gdb used to debug
```c
which gdb
/usr/bin/gdb
```

let gdb position setting variable
Makefile:
```c
GDB = /usr/bin/gdb
```

use `make debug`。use build `kernel.elf` ，which is an object file(not binary) with all the symbols generated on the kernel，must be add `-g` flag，check it with `xxd` and will see some strings。but acually correct way to examine the strings in an object file is `strings kernel.elf`

qemu can co-work with gdb，use `make debug`

1. set breakpoint in `kernel.c:main()`:`b main`
2. run the os:`continue`
3. run two steps，`next` and the other `next`。will see the command set `X`，but not yet show character on the screen
4. see what's in the video memory: `print *video_memor`，there is the "L" from "Landed in 32-bit Protected Mode"
5. make sure that `video_memory` points to the correct address: `print video_memory`
6. `next` to show `X`
7. make sure `print video_memory` and look at the qemu screen

## video-ports

learn how to use the VGA card data ports

examine the I/O ports which map the screen cursor position

will query port `0x3d4` with value `14` to rquest the cursor position high byte and the same port with `15` for the low byte

use gdb to check because still can't print variables on the screen，set a breakpoint for a specific line。

breakpoint kernel.c:21 and use the print command to check variables 

## video-driver

write strings on the screen

can able to output text on the screen

see `drivers/screen.h` and will  see have defined some constants for the VGA card driver and three functions，one to clear and another couple to write strings，last is the famously named `kprint` for kernel print

also see  `drivers/screen.c` ，it's implemenation of the `drivers/screen.h`

there are two I/O port access routinues that，`get` and `set_cursor_offset()`

directory mainipulates the video memory，`print_char()`

### kprint_at

`kprint_at` may be called with a `-1` value for `col` or `row`，which indicates what will print the string at the current cursor posistion

sets three variables for the `col/row` and `offset`.it iterates through the `char*` and calls `print_char()` with the current coordinates

the `print_char` returns the offset of the next cursor position and reuse it for the next loop 

`kprint` is basically a wrapper for `kprint_at`


### print_char

like `kprint_at`，`print_char` allows cols/rows to be `-1 `，get the cursor position from the hardware，using the `ports.c` 

`print_char` also handles newlines。will reset the cursor offset to column 0 of the next row

the VGA cells take `two` bytes，one for character and another one for the color background

### kernel.c

new kernel is able to print strings

have correct character positioning，spanning throug multiple lines，line break。If it tires to write outside of the screen range? it will solve next section

## viedo-scroll

scroll the screen when the text reaches the bottom

see `drivers/screen.c` and note that at the bottom of `print_char` there is a new section (line 84) which checks if the current offset is over the screen size and scrolls the text

scrolling is handled by `memory_copy`，it's simple version of standard `memcpy` but needed named it different name to avoid collsions。see `kernel/util.c` for implementation

To help visualize scrolling，also implement a function to convert integers to text，`int_to_ascii`。this is a quick implementation of the standard function `itoa`。

notice that for integers which have double digits or more，they are printed in reverse

can set a breakpooint  on line 14 on the `kernel.c`



## interrupts

set up the interrupt descriptor table to handle cpu interrupts

### data types
define some specila data types  in `cpu/types.h`，help uncouple data structures for raw bytes from chars and ints，must bee carefully placed on the `cpu/`，will put machine-dependent code now on。the boot code is specifically x86 and is still on `boot/`，but can leave alone for now

some existing files have been  modidfied to use the new `u8`,`u16`,`u32` data types

### interrupts 

interrupts is one of the main point that a kernel need to handle。as soon as possble , to be able to receive keyboard。

another examples of interrupts are: division by zero,out of bounds,invalid opcodes ,page faults ,etc

interrupts are handled on a vector,with entries which are similar to those of the GDT ，but have other name is IDT，do it with c

`cpu/idt.h` defines how an idt entry is stored `idt_gate` (there are need to be 256 of them ，if NULL，or the cpu may panic ) and the actual idt structure that the bios will load,`idt_register` which is just a memory address and size，similar to the GDT register

define a couple variables to access those data structures from assebler code

`cpu/idt.c` fills in every struct with a handler。can see matter of setting the struct values and calling `lidt` assembler command

### ISRs
the interrupt service routines run every time the cpu detects an interrupt which is usually fatal

will write just enough code to handle，print an error message and halt the cpu

on `cpu/isr.h`，define 32 of them，they are declared as `extern` because they will be implemented in assembler，in `cpu/interrupt.asm`


before jumping to the assembler code，check `cpu/isr.c`。can see define a function to install all isrs at once and load the idt， a list of error messages，and the high level handle which kprints some information，can customize `isr_handler` to print/do whatever you want

now to the low level which glues every `idt_gate` with its low-level and high-level handler。open `cpu/interrupt.asm`。define a common low level ISR code which basically saves/restores the state and calls the C code and then the actual ISR assembler functions which are refernced on `cpu/isr.h`

>the `registers_t` struct is a representation of all the registers pushed in `interrupt.asm`

now need to reference `cpu/interrupt.asm` from out Makefile and make the kernel install the ISRs and lauch one of them

notice the cpu doesn't halt after some interrupts
 
## interrupts-irqs

finish the interrupts implemenataion and cpu timer

when the cpu boots,the pic maps IRQs 0-7 to INT 0x8-0xF and IRQs 8-15 to  INT 0x70-0x77

sinece programmed ISRs 0-31,it's standard to remap IRQs to ISRs 32-47

the PICs are communicated with via I/O ports，the master pic has command 0x20 and data 0x21，while the slave has command 0XA0 and data0XA1

code for remapping the PICs is weired and includes some masks，check https://wiki.osdev.org/8259_PIC，otherwise look `cpu/isr.c` ，new code after we set the IDT gates for the ISRs ，after that，add the IDT gates for IRQs

jump to assembler，at `interrupt.asm`，the first task is to add global definitions for the IRQ symbols just used in the C code，look at the end of the `global` statements

The add the IRQ handlers，same `interrupt.asm` at the bottom，notice how to jumpy to a new common stub:`irq_common_stub`

and then create this `irq_common_stub` which is very similar to the ISR one。It is located at the top of `interrupt.asm` andit also defines a new `[extern irq_handler]`

back to c code，to write the `irq_handler()` in `isr.c`，it sent some EOIs to the PICs and calls the handler which is stored in an arary named `interrupt_handlers` and defined at the top of the file。The new structs are defined in `isr.h`，also use a simple function to register the interrupt handlers

we can define our first IRQ handler

no changes in `kernel.c` this time

## interrupts-timer

concept:
1. cpu timer:is master board or internal cpu 's hard component，it make a signal with same frequency

2.keyboard interrupts:interrupt is hardware tells cpu that it's emergcy situation，stop the task right now，deal another task first。

3. scancode is the keyboard hardware sent to computer the raw data

implement  first IRQ handlers: the cpu timer and the keyboard


### Timer

timer is easy to setting。first declare an `init_timer()` on `cpu/timer.h` and implement it on `cpu/timer.c`。a matter of computing the clock frequency and sending the bytes to the appropriate ports

now fix `kernel/utils.c` int_to_ascii() to print the numbers in the correct order。need to implement `reverse()` and `strlen()`

go back to the `kernel/kernel.c` and do two things。enable interrupts again and the initialize the timer interrupt 

`make run` and will see the clock ticking

## keyboard

keyboard setting is very easy，with a drawback，The PIC does not send us the ascii code for the pressed key，but the scancode for the key-press and the key-up events，so will need to translate those

check `drivers/keyboard.c` where there are two function，the callback and the initialization which setting the interrupt callback。A new `keyboard.h` created with the definitons

`keyboad.c` have a long table to translate scancodes to ASCII keys。for the time being we will only implement a simple subset of the US keyboard，can read more https://aeb.win.tue.nl/linux/kbd/scancodes-1.html

## shell

clean the code and parse user input

first clean up the code a bit，try to put things in the most predictable places，it's good exeercise to know when the code is growing  and adapit it to current and furture needs

### code cleaning

will quickly start to need more utility functions for handling strings and others，in a normal os，it's called c library or libc。

now have a `utils.c` which will split into `mem.c` and `string.c`，whih their respective headers

second，will create a new function `irq_install()` that the kernel only needs to perform one call t oinitilize all the IRQs，that function is `isr_install()` and placed on the `isr.c` ，here will disable the `kprint()` on `timer_callback()`  to avoid filling the screen wiht message，now we know that it works

there is not a clear distinction between `cpu/` and `drivers/`，will distinct later，The only change will do for now is to move `drivers/ports.*` into `cpu/` since it's clearly cpu-dependent code。`boot/` is also cpu-dependent code，but we will not mess with it until we implement the boot sequence for a different machine。

there are more sswitches for the `CFLAGS` on the `Makefile`，because will now start creating higher-level functions for our C library and don't want the compiler to include any external code if we make a mistake with a declaration 。also added some flags to turn warnings into errors，since an apparently minor mistake converting pointers can blow up lateron。this also forced us to modify some misc point declarations in our code

finall，will add a marco to avoid warning-errors on unused parameters on `libc/function.h`

### keyboard characters

how to access the typed characters

when a key is pressed，the callback gets the ASCII code via a new arrays which are definied at the beginnig of `keyboard.c`

the callback then appends that character to buffer，`key_buffer`

it's also printed on the screen

when the os wants to read user input,it calls `libc/io.c:readline()`

`keyboard.c` parses backspace，by removing the last element of the key buffer and deleting it from the screen by calling `screen.c:kprint_backspace()`。we neededto modify a bit `print_char()` to not advance the offset when print a backspace


### responding to user input

the keyboard callback checks for a newline，and calls the kernel，tell it the uesr have to input something，our final libc function is `strcmp()` ，compare input string ，if user input `END` ，halt the cpu

## malloc

implement a memory allocator

add kernel memory allocatro `libc/mem.c` implement as a simple pointer to free memory which keeps growing

The `kmalloc()` function can be used to request an aligned page and it also  return the real physical address for later use

change the `kernel.c` leaving all the shell code，just try out the `kmalloc()` and check out first page starts at 0x10000 (as hardcoded on `mem.c`) and subsequent `kmalloc()`'s produce a new address which is aligned 4096 bytes or 0x10000 from the previous one

note added a new strings.c:`hex_to_ascii()` for  printing fo hex numbers

simple revise rename `types.h` to `type.h` for language  consistency


