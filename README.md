# os

## 作業系統參考資料
https://github.com/cfenollosa/os-tutorial/tree/master
https://wiki.osdev.org/Expanded_Main_Page

## 待實現功能

[x]tab補全  
[x]命令歷史記錄(上下鍵瀏覽) 
[x]CLEAR：清空螢幕  
[x]TIME：顯示系統時間
[x]ECHO：回顯文字  
[x]CALC：簡單計算器  
[]分頁機制(Paging)   
[x]實作 kfree()（釋放記憶體）  
[]設計檔案系統結構（FAT12/簡化版）  
[x]Calling Global Constructors
[x]printf相關函式

## 細節修改
time:已修正為顯示台灣時區


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

學習如何使用 16 位元真實模式分段來定址記憶體

之前使用 `[org]` 做過分段

分段意味著您可以為所有引用的資料指定一個偏移量

使用 `cs`、`ds`、`ss`、`es` 分別對應程式碼、資料、堆疊、額外段

警告：這些暫存器會被 CPU 隱式使用，因此所有記憶體存取都會被 `ds` 偏移

計算真實位址的公式為 `segment * 16 + address`（即 `segment << 4 + address`）。例如，如果 `ds` 是 `0x4d`，那麼 `[0x20]` 實際上指的是 `0x4d * 16 + 0x20 = 0x4d0 + 0x20 = 0x4f0`

注意：不能直接使用 `mov` 將立即數值載入這些暫存器，必須先使用通用暫存器作為中介

編譯指令
```c
nasm -f bin boot_sect_segmentation.asm  -o boot_sect_segmentation.bin
```

執行指令
```c
qemu-system-x86_64 boot_sect_segmentation.bin
```

## bootsector-disk

讓啟動扇區從磁碟載入資料以便啟動核心

作業系統無法完全放入啟動扇區的 512 字節中，需要從磁碟讀取資料來執行核心

不需要處理磁碟旋轉盤片的開關等硬體細節

可以直接呼叫 BIOS 常式。要這樣做，將 `ah` 設為 `0x02`（讀取扇區功能），並設定其他暫存器（包含所需的磁柱、磁頭和扇區參數），然後觸發 `int 0x13`

更多 INT 13h 詳細資訊：https://stanislavs.org/helppc/int_13-2.html

這次會使用進位標誌（carry flag），它是 EFLAGS 暫存器中的一個標誌位，當運算發生進位或借位時會被設定

```c
mov ax, 0xFFFF
add ax, 1 ; ax = 0x0000 and carry = 1
```

進位標誌不能直接存取，但會被其他指令作為控制位元使用，例如 `jc`（如果進位標誌被設定則跳轉）

BIOS 會在 `al` 中設定實際讀取的扇區數，務必將其與預期的扇區數進行比較

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

在 32 位元保護模式下在螢幕上列印

32 位元模式可以使用 32 位元暫存器和記憶體定址、保護記憶體、虛擬記憶體，但會失去 BIOS 中斷並需要編寫 GDT

編寫在 32 位元模式下運作的列印字串迴圈，不需要 BIOS 中斷

直接操作 VGA 視訊記憶體，而不是呼叫 `int 0x10`

VGA 記憶體位於位址 `0xb8000`，具有文字模式以避免直接操作像素

存取 80x25 網格上特定字元的公式：
`0xb8000 + 2 * (row * 80 + col)`

每個字元使用 2 字節（一個用於 ASCII，另一個用於顏色）

## 32bit-gdt

分段操作方式被左移以定址額外一層的間接定址

在 32 位元模式下，分段運作方式不同，偏移量是 GDT 中段描述符（Segment Descriptor, SD）的索引。這個描述符定義了基底位址（32 位元）、大小（20 位元）以及一些標誌，例如唯讀、權限等

編寫 GDT 的簡單方法是定義兩個段，一個用於程式碼，另一個用於資料，這些段可以重疊，這意味著沒有記憶體保護，但足以啟動系統

第一個 GDT 條目必須是空描述符（null descriptor，全為 0），以確保程式設計師不會在管理位址時犯錯

CPU 無法直接載入 GDT 位址，它需要一個稱為「GDT 描述符」的中繼結構，包含實際 GDT 的大小（16 位元）和位址（32 位元）。它透過 `lgdt` 指令載入

注意：參考 os-dev.pdf 檢查段標誌

## 32bit-enter

進入 32 位元保護模式並測試之前的程式碼

進入 32 位元模式的步驟：
1. 禁用中斷
2. 載入 GDT
3. 設定 CPU 控制暫存器 `cr0` 中的一個位元
4. 透過執行一個精心設計的遠跳轉來清除 CPU 管線
5. 更新所有段暫存器(ds,ss,es,fs,gs)
6. 更新堆疊
7. 呼叫一個已知的標籤，該標籤包含第一個有用的 32 位元程式碼

在檔案 `32bit-switch.asm` 中建立此流程

進入 32 位元模式後，會呼叫 `BEGIN_PM`，這是實際有用程式碼的進入點，可以查看 `32bit-main.asm`

compile instruction
```c
nasm -f bin 32bit-main.asm  -o 32bit-main.bin
```

usage
```c
qemu-system-x86_64 32bit-main.bin
```


## kernel-crosscompiler

建立開發環境以建置您的核心

一旦跳轉到使用高階語言開發時，需要一個交叉編譯器，這裡使用 C

首先安裝所需的套件：
- gmp
- mpfr
- libmpc
- gcc

需要 `gcc` 來建置交叉編譯器

```c
export CC=/usr/local/bin/gcc
export LD=/usr/local/bin/gcc
```

需要建置 binutils 和交叉編譯的 gcc，會將它們放入 `/usr/local/i386elfgcc`

```c
export PREFIX="/usr/local/i386elfgcc"
export TARGET=i386-elf
export PATH="$PREFIX/bin:$PATH"
```

### binutils
執行下面命令，但要有root權限才能執行
>備註:建議下載更新的binutils.tar.gz，會比較好，這os還是用2.24
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
一樣，要有root權限才能執行
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

安裝完後應該要有全部的GNU binutils和編譯器在`/usr/local/i386elfgcc/bin`

## kernel-c

學習如何用 C 語言撰寫低階程式碼，就像我們用組合語言做的一樣

### Compile

查看 C 編譯器如何編譯我們的程式碼，並與組合語言產生的機器碼進行比較

開始撰寫一個簡單的程式，包含一個函數 `function.c`

要編譯系統獨立的程式碼，需要 `-ffreestanding` 旗標

`-ffreestanding`：非常重要，讓編譯器知道它正在建置核心而不是使用者空間程式。需要實作自己的 `memset`、`memcpy`、`memcmp` 和 `memmove` 函數

compile instruction
```c
i386-elf-gcc -ffreestanding -c function.c -o function.o
```

用obdjump檢查機器碼
```c
i386-elf-objdump -d function.o
```

## Link

要產生二進制檔案，使用連結器，重要的部分是學習高階語言如何呼叫函數標籤

函數在記憶體中會被放置在什麼偏移量？

我們實際上不知道。例如，我們會將偏移量設為 `0x0`，並使用 `binary` 格式，該格式會產生不包含任何標籤和元資料的機器碼

```c
i386-elf-ld -o function.bin -Ttext 0x0 --oformat binary function.o
```
連結時可能會出現警告，可以忽略它

使用 `xxd` 檢查 `function.o` 和 `function.bin` 這兩個檔案

可以看到 `.bin` 檔案是純機器碼

`.o` 檔案包含許多除錯資訊和標籤

## 反編譯程式
檢查機器碼
```c
ndisasm -b 32 function.bin
```

## more program

小型程式，包含以下特性：
1. 區域變數 `localvars.c`
2. 函數呼叫 `functioncalls.c`
3. 指標 `pointers.c`

然後編譯並反組譯它們，檢查產生的機器碼


### compile program
```c
i386-elf-gcc -ffreestanding -c localvars.c -o localvars.o
i386-elf-gcc -ffreestanding -c functioncalls.c -o functioncalls.o
i386-elf-gcc -ffreestanding -c pointers.c -o pointers.o
```

用`i386-elf-objdump` 檢查
```c
i386-elf-objdump -d localvars.o
i386-elf-objdump -d functioncalls.o
i386-elf-objdump -d pointers.o
```

### 連接程式
```c
i386-elf-ld -o localvars.bin -Ttext 0x0 --oformat binary localvars.o
i386-elf-ld -o functioncalls.bin -Ttext 0x0 --oformat binary functioncalls.o
i386-elf-ld -o pointers.bin -Ttext 0x0 --oformat binary pointers.o
```


用xxd檢查
```c
xxd localvars.o
xxd localvars.bin
xxd functioncalls.o
xxd functioncalls.bin
xxd pointers.o
xxd pointers.bin
```

### 反編譯程式
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

建立一個簡單的核心（kernel）以及一個能夠啟動該核心的開機磁區（boot sector）

### kernel
一個 16-bit boot sector（組語） 載入並跳轉到一個 C kernel，而該 kernel 只做一件事——在螢幕左上角印出一個 X。
同時也會包含你特別提到的 dummy function，用來強迫 kernel 入口不是位於 0x0，而是明確的 main() 標籤。

compile instruction
```c
/usr/local/i386elfgcc/bin/i386-elf-gcc -ffreestanding -c kernel.c -o kernel.o
```

例程程式碼為 `kernel_entry.asm`。將學習如何在組合語言中使用 `[extern]` 宣告。此檔案不會編譯成二進位檔（binary），而是產生 ELF 格式，並與 `kernel.o` 進行連結。

compile instruction
```c
nasm kernel_entry.asm -f elf -o kernel_entry.o
```

### the linker

連結器（linker）是一個非常有用的工具。

它可以將多個目的檔（object files）連結成單一的核心二進位檔（kernel binary），並解析各種標籤（label）的參照。

核心不會被放置在記憶體的 `0x0`位址，而是放在 `0x1000`。開機磁區（boot sector）也必須知道這個位址。

compile instruction
```c
/usr/local/i386elfgcc/bin/i386-elf-ld -o kernel.bin -Ttext 0x1000 kernel_entry.o kernel.o --oformat binary
```

### the bootsector
`bootsector.asm`，並檢視其程式碼


compile instruction
```c
nasm bootsect.asm -f bin -o bootsect.bin
```


### putting it all together

為開機磁區（bootsector）與核心（kernel）各自使用兩個獨立的檔案，  再透過連結（link）將它們合併成單一檔案。


concatenate instruction
```c
cat bootsect.bin kernel.bin > os-image.bin
```

### run os-image.bin
如果發生磁碟載入錯誤，可能需要在 QEMU 中加入軟碟參數（floppy = `0x0`，硬碟 = `0x80`）。

```c
qemu-system-i386 -fda os-image.bin  //from floopy 
```

不能使用 `qemu-system-i386 -hda os-image.bin` 從硬碟啟動作業系統，因為 `os-image.bin` 的容量太小。


會看到四則訊息：
- "Started in 16-bit Real Mode"
- "Loading kernel into memory"
- （左上角）"Landed in 32-bit Protected Mode"
- （左上角，覆蓋先前訊息）"X"



## check_point

學習如何使用 GDB 偵錯核心（kernel）。

已經執行了自己的核心，但功能非常簡單，只會印出一個 'X'。

install gdb use to debug
```c
which gdb
/usr/bin/gdb
```

let gdb position setting variable
Makefile:
```c
GDB = /usr/bin/gdb
```

使用 `make debug`。會建立 `kernel.elf`，這是一個物件檔（不是二進位檔），包含核心中所有產生的符號。  
編譯時必須加上 `-g` 旗標。可以用 `xxd` 檢查，會看到一些字串，但檢查物件檔中字串的正確方法是 `strings kernel.elf`。

QEMU 可以與 GDB 配合使用，使用 `make debug`：

1. 在 `kernel.c:main()` 設定斷點：`b main`
2. 執行作業系統：`continue`
3. 執行兩個步驟：`next` 再 `next`，會看到指令已經設定 'X'，但螢幕上尚未顯示字元
4. 查看影片記憶體內容：`print *video_memory`，會看到 "L"，來自 "Landed in 32-bit Protected Mode"
5. 確認 `video_memory` 指向正確位址：`print video_memory`
6. `next`，讓 'X' 顯示在螢幕上
7. 再次確認：`print video_memory`，並觀察 QEMU 螢幕



## video-ports

學習如何使用 VGA 顯示卡的資料埠（data ports）。

檢查對應螢幕游標位置的 I/O 埠。

- 檢查port `0x3D4`，設定值為 `14` 以取得游標位置的高位元組  
- 同一埠設定值為 `15` 以取得低位元組

因為還無法直接在螢幕上印出變數，所以使用 GDB 來檢查，並在特定程式行設置斷點。

- 在 `kernel.c:21` 設置斷點，使用 `print` 指令來檢查變數


## video-driver

在螢幕上寫入字串。

可以在螢幕上輸出文字。

查看 `drivers/screen.h`，會看到定義了一些 VGA 顯示卡驅動的常數，以及三個函式：  
- 一個用來清除螢幕  
- 另外兩個用來寫入字串  
- 最後一個著名的 `kprint`，用於核心輸出（kernel print）

也可以查看 `drivers/screen.c`，這是 `drivers/screen.h` 的實作檔案。

其中有兩個 I/O 埠存取例程：`set_cursor_offset()` 與 `set_cursor_offset()`。

直接操作影像記憶體，使用 `print_char()` 函式。


### kprint_at

`kprint_at` 可能會被呼叫時傳入 `-1` 作為 `col` 或 `row` 的值，這表示字串將會從目前游標位置開始印出。

會設定三個變數：`col`、`row` 和 `offset`。  
函式會逐一遍歷字元指標 `char*`，並以當前座標呼叫 `print_char()`。

`print_char` 會回傳下一個游標位置的 offset，並在下一個迴圈中重複使用。

`kprint` 基本上是 `kprint_at` 的封裝（wrapper）。



### print_char

像 `kprint_at` 一樣，`print_char` 也允許 `col` / `row` 為 `-1`，此時會從硬體取得游標位置，使用 `ports.c`。

`print_char` 也會處理換行（newline），將游標 offset 重設到下一行的第 0 欄。

VGA 的每個儲存格佔用 `兩個`位元組，一個用於字元，另一個用於顏色與背景。


### kernel.c

新的kernel已經可以印出字串。

具有正確的字元定位，能跨越多行並處理換行(`\n`)。  
如果嘗試寫入螢幕範圍之外的字元？這部分將在下一節解決。


## video-scroll

當文字到達螢幕底部時捲動螢幕

請參閱 `drivers/screen.c`，並注意到 `print_char` 的底部有一個新區段（約在第 101 行），它會檢查目前的偏移量（offset）是否超過螢幕大小，並進行文字捲動。

捲動是由 `memory_copy` 處理的，它是標準 `memcpy` 的簡化版本，但為了避免名稱衝突而取了不同的名字。實作請參閱 `libc/mem.c`（原為 `kernel/util.c`）。

為了幫助視覺化捲動，我們還實作了一個將整數轉換為文字的函數 `int_to_ascii`。這是標準函數 `itoa` 的快速實作。

請注意，兩位數以上的整數在目前的實作中已經可以正常顯示（先前版本曾有反向列印的問題）。

可在 `kernel/kernel.c` 的第 14 行設置斷點。




## interrupts (中斷)

設置中斷描述表 (Interrupt Descriptor Table, IDT) 以處理 CPU 中斷。

### 資料型別 (Data Types)
我們在 `cpu/` 目錄下整合機器相關的程式碼，並使用明確定義大小的資料型別，這有助於將底層位元組結構與一般的字元或整數解耦。雖然早期曾使用自定義的 `u8`, `u16`, `u32` (原定義於 `cpu/types.h`)，但目前已全面改用 `<stdint.h>` 標準型別（如 `uint8_t`, `uint32_t`）以符規範。

相關的開機程式碼 (boot code) 則是 x86 特有的，目前仍保留在 `boot/` 目錄中。

### interrupts (中斷處理)

中斷是核心必須處理的核心要點之一。我們需要儘快建立此機制，以便能夠接收鍵盤輸入。


其他中斷範例包括：除以零 (division by zero)、越界 (out of bounds)、無效指令 (invalid opcodes)、分頁錯誤 (page faults) 等。

中斷是透過一個「向量表」(vector) 來處理的，其條目與 GDT 類似，但在中斷機制中稱為 IDT (中斷描述表)。我們將使用 C 語言來實作。

`cpu/idt.h` 定義了 IDT 條目的儲存方式 `idt_gate_t`（必須定義 256 個條目，如果為空，CPU 可能會發生崩潰/Panic）以及供 CPU 載入的實體 IDT 結構 `idt_register_t`。後者僅包含記憶體位址與大小，與 GDT 暫存器類似。

我們定義了一些變數，以便從組合語言 (assembler) 存取這些資料結構。

`cpu/idt.c` 將每個結構填入對應的處理程序 (handler)。你可以看到這涉及設定結構值並呼叫 `lidt` 組合語言指令。


### ISRs (中斷服務常式)
每當 CPU 偵測到中斷（通常是致命錯誤）時，就會執行中斷服務常式 (Interrupt Service Routines, ISR)。

我們將編寫最精簡的處理代碼：印出一條錯誤訊息並停止 CPU。

在 `cpu/isr.h` 中定義了 32 個 ISR，它們被宣告為 `extern`，因為它們將在組合語言檔案 `cpu/interrupt.asm` 中實作。

在進入組合語言程式碼之前，請先查看 `cpu/isr.c`。你可以看到這裡定義了一個函式來一次安裝所有 ISR 並載入 IDT、一份錯誤訊息列表，以及顯示資訊的高階處理程序 (high level handler)。你可以根據需求自訂 `isr_handler` 以印出或執行任何操作。

接著是低階部分，它將每個 `idt_gate` 與其對應的低階和高階處理程序連結起來。打開 `cpu/interrupt.asm`，我們定義了一段通用的低階 ISR 程式碼，主要負責儲存/還原狀態並呼叫 C 語言代碼，以及在 `cpu/isr.h` 中引用的實際 ISR 組合語言函式。

> `registers_t` 結構是 `interrupt.asm` 中推入堆疊的所有暫存器的表現形式。

現在需要在我們的 `Makefile` 中引用 `cpu/interrupt.asm`，並讓核心安裝 ISR 並啟動其中一個進行測試。

請注意，目前有些中斷觸發後 CPU 並不會停止（halt）。

 
### interrupts-irqs (中斷-IRQ)


當 CPU 啟動時，可程式化中斷控制器 (PIC) 預設將 IRQ 0-7 映射到 INT 0x8-0xF，將 IRQ 8-15 映射到 INT 0x70-0x77。

由於我們已經編寫了 ISR 0-31 來處理 CPU 異常，因此標準做法是將 IRQ 重新映射到 ISR 32-47。

我們透過 I/O 埠與 PIC 通訊：主 (Master) PIC 的命令埠為 `0x20`，資料埠為 `0x21`；從 (Slave) PIC 的命令埠為 `0xA0`，資料埠為 `0xA1`。

重新映射 PIC 的代碼較為特殊且包含一些掩碼 (masks)，詳情可以參考 [OSDev Wiki](https://wiki.osdev.org/8259_PIC)，或者查看 `cpu/isr.c`：在設定完異常處理的 IDT 門 (gates) 之後，緊接著就是 IRQ 的 IDT 門設定。

跳轉到組合語言部分，在 `interrupt.asm` 中，第一個任務是為 C 代碼中使用的 IRQ 符號新增全域定義（請見 `global` 語句的末尾）。

接著在 `interrupt.asm` 的底部新增 IRQ 處理程序，注意到它們會跳轉到一個新的通用 stub：`irq_common_stub`。

然後建立這個 `irq_common_stub`，它與 ISR 的版本非常相似。它位於 `interrupt.asm` 的頂部，並宣告了一個新的 `[extern irq_handler]`。

回到 C 程式碼，在 `isr.c` 中編寫 `irq_handler()`：它負責向 PIC 發送中斷結束訊號 (EOI)，並呼叫儲存在 `interrupt_handlers` 陣列（定義於檔案頂部）中的處理程序。相關結構定義在 `isr.h` 中，我們還使用了一個簡單的函式來註冊中斷處理程序。

現在我們可以定義第一個 IRQ 處理程序了。

本次 `kernel.c` 不需要任何變更。


## interrupts-timer

基本概念：
1. CPU 計時器（CPU Timer）：是主機板或 CPU 內部的硬體組件，能以固定頻率產生訊號。

2. 鍵盤中斷（Keyboard Interrupts）：中斷是硬體用來告知 CPU 發生了緊急情況，需立即停止當前任務並優先處理另一項任務的機制。

3. 掃描碼（Scancode）：這是鍵盤硬體傳送到電腦的原始數據。

實作首批 IRQ 處理程序：CPU 計時器與鍵盤。


### Timer (計時器)

計時器的設定非常簡單。首先在 `cpu/timer.h` 宣告 `init_timer()` 並在 `cpu/timer.c` 實作。這主要涉及計算時鐘頻率並將位元組發送到對應的埠口。

接著修正 `libc/string.c`（原為 `kernel/utils.c`）中的 `int_to_ascii()`，使其能按正確順序印出數字。為此我們需要實作 `reverse()` 與 `strlen()`。

回到 `kernel/kernel.c` 執行兩件事：重新啟用中斷（在 `irq_install` 中執行）並初始化計時器中斷。

執行 `make run` 即可看到時鐘跳動（若處理程序中有印出訊息）。


## keyboard

鍵盤設定非常簡單，但有一個缺點：PIC 傳送的不是按鍵的 ASCII 碼，而是按鍵（key-press）與放開（key-up）事件的掃描碼（scancode），因此我們需要進行轉換。

請參閱 `drivers/keyboard.c`，其中包含兩個函式：回呼函式（callback）與設定中斷回呼的初始化函式。同時也建立了包含相關定義的 `keyboard.h`。

`keyboard.c` 中有一張長表用於將掃描碼轉換為 ASCII 碼。目前我們僅實作了美式鍵盤（US keyboard）的一個簡單子集，詳細資訊請參閱 [鍵盤掃描碼參考資料](https://aeb.win.tue.nl/linux/kbd/scancodes-1.html)。


## shell

先整理程式碼，再解析使用者輸入。

首先稍微清理一下程式碼，嘗試將各個程式模組放在最合理且易於預期的位置。這是一個很好的練習，能讓我們覺察程式碼何時開始過度增長，並調整架構以適應當前與未來的需求。


### code cleaning

我們很快就會需要更多處理字串及其他功能的工具函式，在標準的作業系統中，這被稱為 C 函式庫或 libc。

現在我們將原本的 `utils.c` 拆分為 `mem.c` 與 `string.c`（位於 `libc/` 目錄下），並附帶各自的標頭檔。

其次，我們建立了一個新的函式 `irq_install()`，讓核心只需呼叫一次即可初始化所有 IRQ。相對應地，初始化異常處理的函式為 `isr_install()`，兩者皆位於 `isr.c`。在此階段，我們會停用 `timer_callback()` 中的 `kprint()` 訊息，以避免時鐘跳動訊息填滿螢幕。

目前 `cpu/` 與 `drivers/` 之間的劃分尚不完全明確，日後會再優化。目前的變動是將 `drivers/ports.*` 移入 `cpu/`，因為埠口操作顯然屬於 CPU 相關代碼。`boot/` 同樣也屬於 CPU 相關代碼，但除非未來要支援其他硬體架構，否則暫不更動。

`Makefile` 中的 `CFLAGS` 增加了更多編譯旗標。這是因為我們開始撰寫高階函式，不希望編譯器在處理宣告時引入任何外部連結。我們也加入了將警告視為錯誤的設定，因為指標轉型上的微小失誤往往是後續嚴重錯誤的根源，這也促使我們修正了程式碼中一些不嚴謹的指標宣告。

最後，我們在 `libc/function.h` 中加入了一個宏 (macro)，用於消除編譯器針對「未使用參數」產生的警告錯誤。


### keyboard characters

如何存取鍵盤輸入的字元：

當按鍵被按下時，回呼函式（callback）會透過 `keyboard.c` 開頭定義的新陣列（如 `sc_ascii`）獲取對應的 ASCII 碼。

隨後，回呼函式會將該字元追加到緩衝區 `key_buffer` 中。

該字元也會同步顯示在螢幕上。

當使用者按下回車鍵（Enter）時，系統會呼叫核心函式 `user_input(key_buffer)` 來處理輸入內容。

`keyboard.c` 處理退格鍵（Backspace）的方式是刪除 `key_buffer` 中的最後一個字元，並透過重新顯示輸入行來更新螢幕（這涉及呼叫 `screen.c:kprint_backspace()`）。我們也對 `print_char()` 進行了微調，使其在遇到退格符號（0x08）時不會增加偏移量（offset）。



### responding to user input

鍵盤回呼函式（callback）會檢查換行符號，並呼叫核心告知使用者已完成了輸入。我們最後一個 libc 函式是 `strcmp()`，用於比較輸入字串。如果使用者輸入 `end`，則停止 CPU 運行。

## malloc

實作記憶體分配器。

在 `libc/mem.c` 中加入核心記憶體分配器。其實作方式為一個指向可用記憶體的簡單指標，該指標會隨著分配不斷增長。

`kmalloc()` 函式可用於請求對齊的分頁（aligned page），並且它也會回傳用於後續用途的實體位址。

修改 `kernel.c`，保留所有 Shell 相關程式碼，僅加入對 `kmalloc()` 的測試。確認第一頁是從 `0x10000` 開始（此處在 `mem.c` 中為硬編碼），隨後的 `kmalloc()` 呼叫會產生新的位址，且該位址與前一個位址對齊 4096 位元組（或 `0x1000`）。

注意：在 `libc/string.c` 中新增了 `hex_to_ascii()` 函式，用於印出十六進位數字。

簡單修正：為了語言一致性，將 `types.h` 重新命名為 `type.h`。


## fixes

修正雜項問題

OSDev Wiki 有一個頁面 [James Molloy's Tutorial Known Bugs](https://wiki.osdev.org/James_Molloy%27s_Tutorial_Known_Bugs)。由於我們遵循了該教學（從中斷到 malloc），因此需要確保修正以下問題：

### 1. 錯誤的 CFLAGS
在編譯 `.o` 檔案（包括 `kernel_entry.o`、`kernel.bin` 和 `os-image.bin` 相關物件）時，加入 `-ffreestanding` 旗標。

先前我們透過 `-nostdlib` 停用了 `libgcc`（注意不是 `libc`），但在連結時沒有重新啟用它，這會變得很棘手。因此我們在目前版本中調整了參數，確保正確連結必要的獨立程式庫。

同時也向 gcc 傳遞了 `-nostdinc`。

### 2. kernel.c 的 `main()` 函式
修改 `kernel/kernel.c`，將 `main()` 改名為 `kernel_main()`，因為 gcc 將 `main` 視為特殊關鍵字，我們不應直接操作它。

相應地修改 `boot/kernel_entry.asm` 以指向新名稱。

修正 `i386-elf-ld: warning: cannot find entry symbol _start; defaulting to 0000000000001000` 警告訊息：在 `boot/kernel_entry.asm` 中加入 `global _start;` 並定義 `_start:` 標籤。

### 3. 重塑資料型別
定義非標準資料型別（如 `u32` 等）並非好主意，因為 C99 引入了標準的固定寬度資料型別（如 `uint32_t`）。

我們改為引入 `<stdint.h>`，這在 `-ffreestanding` 模式下依然有效（但需要環境支援），使用標準型別取代自定義型別，並刪除 `type.h`。

同時刪除不必要的 `__asm__` 和 `__volatile__` 關鍵字（除非確實需要）。

### 4. `kmalloc` 對齊與型別問題
由於 `kmalloc` 使用大小參數，應使用正確的資料型別 `size_t` 取代 `u32int_t`（或 `uint32_t`）。所有相關參數都應統一使用 `size_t`。

### 5. 缺失的函式
實作缺失的 `mem*` 系列函式（如 `memory_copy`、`memory_set`）。

### 6. 中斷處理程序 (Interrupt Handlers)
`cli` 指令是多餘的，因為如果 IDT 條目（`idt_gate_t`）的標誌設定正確，中斷會在進入處理程序時自動禁用。

`sti` 也是多餘的，因為 `iret` 指令會從堆疊中彈出其儲存的 `EFLAGS` 值，其中包含了中斷是否開啟的位元。換句話說，中斷處理程序會自動恢復到中斷發生前的狀態（無論之前是否啟用了中斷）。

在 `cpu/isr.h` 中，`struct registers_t` 有多處問題。首先，原本的 `esp` 被重新命名為 `useless`，因為該值反映的是當前的堆疊上下文，而非被中斷時的狀態。因此我們將原有的 `useresp` 重新命名為 `esp`。

根據 OSDev Wiki 的建議，在 `cpu/interrupt.asm` 呼叫 `isr_handler` 之前加入 `cld` 指令。

`cpu/interrupt.asm` 的另一個重要修正：通用的 stub 會在堆疊上建立 `struct registers_t` 的實體並呼叫 C 處理程序。但這違反了 ABI 規範，因為堆疊空間屬於被呼叫函式，它們可以隨意更改其值。我們必須以指標（pointer）的形式傳遞該結構。

實作方法：
1. 修改 `cpu/isr.h` 和 `cpu/isr.c`，將 `registers_t r` 改為 `registers_t *t`。
2. 將結構欄位的存取方式從 `.` 改為 `->`。
3. 在 `cpu/interrupt.asm` 呼叫 `isr_handler` 和 `irq_handler` 之前加入 `push esp`（推入結構位址）。
4. 呼叫結束後記得 `pop eax` 以清理該指標。

目前所有的回呼函式（如計時器和鍵盤）也都需要修改為使用 `registers_t` 指標。


## 更新cross-compiler版本

原本教學是到El Capitan，查了才知道這是mac的電腦版本，才需要在更新原本的cross-compiler。基本上跟之前的教學沒啥兩樣，算是最後的章節了

## rtc

實作實時時鐘 (Real Time Clock, RTC) 驅動，主要透過 CMOS I/O 埠存取硬體時間資訊。

### CMOS 埠口存取
* **Index Port (0x70)**: 用於指定要存取的暫存器索引。
* **Data Port (0x71)**: 用於讀取或寫入指定暫存器的資料。

> **注意**: 在存取 Port 0x70 時，最高位元 (bit 7) 是 **NMI Disable** 位元。將其設為 1 會暫時停用不可遮蔽中斷 (NMI)，許多開源系統在存取 CMOS 時會設定此位元以確保操作原子性。

### 關鍵暫存器與資料格式
RTC 提供暫存器來儲存秒、分、時、日、月、年等其他資訊：
* `0x00`: 秒
* `0x02`: 分
* `0x04`: 時
* `0x07`: 日
* `0x08`: 月
* `0x09`: 年
* `0x0A (Status Register A)`: 
    * **Bit 7 (UIP)**: Update In Progress。若此位元為 1，表示 RTC 正在更新時間，此時讀取的值可能無效，驅動程式必須等待其變為 0。
* `0x0B (Status Register B)`:
有四種模式
    * binary 模式 或 BCD 模式
    * 12 小時制模式 或 24 小時制模式
有些format bit在register B中無法改變，所以必須處理這四種可能。並且別試著改變register B中的值，必須先讀取register B中的值，找到你要的格式，再進行處理。

- 在register B中，bit 1(value=2)是代表24小時制
- 在register B中，bit 2(value=4)是代表binary模式

binary mode就是預期的正常時間，假設時間是1:59:48 AM，hours的值會是1，minutes的值是59=0x3b，seconds的值是48=0x30

在BCD mode中，每一對16進位的byte將被修改成顯示十進位的數字，所以1:59:48 AM 會有hours是1，minutes的值是0x59=89，seconds的值是0x48=72，為了轉換成二進位制，需要以下公式來轉換。binary =((bcd/16)*10)+(bcd & 0xf)。優化公式版本binary = ( (bcd & 0xF0) >> 1) + ( (bcd & 0xF0) >> 3) + (bcd & 0xf)。


12小時制轉換24小時有點麻煩，如果hour是pm，那0x80 bit將被設為1，所以必須遮罩掉，然後午夜12，1 am是1，注意午夜不是0，是12，這是需要處理12小時制到24小時制的特殊情況(設置12為0)


### 驅動實作細節
1. **安全讀取機制 - 二次比對法 (Double Read)**: 
    * 僅檢查 `0x0A` 的 UIP 位元是不夠的，因為讀取多個暫存器期間可能剛好發生進位。
    * **正確做法**: 先檢查 UIP 為 0，讀取所有時間暫存器，接著再次檢查並讀取。若兩次讀取的結果完全相同，才視為有效資料。若不相同則重複此過程。
2. **週期性中斷 (可選)**: 若需要高頻率計時，可以設定 Register B 的 PIE 位元，RTC 會觸發 **IRQ 8** 中斷。中斷頻率透過 Register A 的低 4 位元 (Rate Selection) 控制。
3. **優化bcd to binary**

### 參考資料：
* [OSDev CMOS Wiki](https://wiki.osdev.org/CMOS)
* [OSDev RTC Wiki](https://wiki.osdev.org/RTC)


## PIT(Programmable Interval Timer)

PIT (Programmable Interval Timer) 是早期 IBM PC 中負責系統計時、硬體中斷觸發與音訊產生的核心組件。

## 核心規格
基準頻率：約 1.193182 MHz (由 14.31818 MHz 基準時脈 12 分頻而來)。
組成架構：包含 1 個振盪器、1 個預分頻器及 3 個獨立的 16 位元分頻器（通道）。
計數能力：16 位元計數器，範圍 0~65535。其中 0 代表 65536。
精確度：誤差約為每日 +/- 1.73 秒。

## 通道分配 (Channels)
通道	輸出連接	用途說明
Channel 0	IRQ 0	系統心跳：產生最高優先權中斷，用於作業系統排程。預設頻率 18.2 Hz。
Channel 1	DRAM 控制器	記憶體重新整理：現代電腦中已由專用硬體取代，通常已廢棄。
Channel 2	PC 喇叭	音訊產生：唯一可由軟體透過 I/O 埠 0x61 控制 Gate 與讀取輸出的通道。

## I/O 埠配置
I/O 埠	用途	說明
0x40	通道 0 資料埠	讀取/設定 Channel 0 計數值。
0x41	通道 1 資料埠	讀取/設定 Channel 1 計數值 (現代環境少用)。
0x42	通道 2 資料埠	讀取/設定 Channel 2 計數值。
0x43	模式/命令暫存器	唯寫。用於設定存取模式、運作模式及鎖存指令。

## 模式暫存器 (0x43) 格式
位元	用途	常用設定值
7-6	選擇通道	00=Ch0, 10=Ch2
5-4	存取模式	00=鎖存(Latch), 11=先傳低位再傳高位(Lobyte/Hibyte)
3-1	運作模式	010=Mode 2 (速率產生器), 011=Mode 3 (方波產生器)，還有其他模式
0	BCD/二進位	0=16位元二進位 (只用16位元二進位)

## 常見運作模式
Mode 2 (Rate Generator)：產生極短的低電位脈衝。適合高精度的系統時鐘計時。
Mode 3 (Square Wave)：產生 50% 佔空比方波。PC 喇叭發聲的標準模式。

## 喇叭發聲設定 (PC Speaker)
控制 PC 喇叭發聲需要同時操作 **PIT Channel 2 (0x42)** 與 **系統控制埠 B (0x61)**。

### 步驟 1：設定 PIT Channel 2 頻率
要讓喇叭發出特定頻率的聲音，必須將 Channel 2 設定為 **Mode 3 (方波產生器)**。
1.  輸出的控制位元組為 `0xB6` (1011 0110):
    *   `10`: 選擇 Channel 2
    *   `11`: 先傳低位再傳高位 (Lobyte/Hibyte)
    *   `011`: Mode 3 (方波)
    *   `0`: 二進位計數
2.  計算除數 (Divisor) = `1193180 / 頻率 (Hz)`。
3.  操作埠口：
    *   將 `0xB6` 寫入 `0x43`。
    *   將除數低位寫入 `0x42`。
    *   將除數高位寫入 `0x42`。

### 步驟 2：開啟喇叭開關 (Port 0x61)
PC 喇叭的物理連通受 **System Control Port B (0x61)** 控制：
*   **Bit 0**: 開啟後，PIT Channel 2 的輸出才會送到喇叭。
*   **Bit 1**: 開啟後，喇叭發聲器才會啟動。

**程式操作：**
*   **播放聲音**：讀取 `0x61` 的當前值，將 Bit 0 和 Bit 1 設為 1 (即 `value | 0x03`) 後寫回。
*   **停止發聲**：讀取 `0x61` 的當前值，將 Bit 0 和 Bit 1 設為 0 (即 `value & ~0x03`) 後寫回。

---

## PIT 原子操作與鎖存 (Latch)
在讀取 16 位設計數器時，為避免讀取過程中計數值發生變化導致錯誤，應使用 **鎖存指令 (Latch Command)**：
1.  寫入 `0x43`：Bits 5-4 設為 `00` (Latch)，Bits 7-6 選擇通道。
2.  讀取對應通道資料埠：連續讀取兩次 (低位、高位)。

### 參考資料
https://wiki.osdev.org/Programmable_Interval_Timer


