# os

# bootsector

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
