# 操作與複習練習

[回首頁](../README.md) · [架構](architecture.md) · [概念](concepts.md) · [追蹤](traces.md)

先看一段程式、預測結果，再操作驗證。以下預期來自目前原始碼；自動測試範圍見[保護驗證](user-protection.md#自動驗證)，未列入的 history、Tab 等互動仍可依下表手動練習。

## 建置與啟動

在 repo 根目錄執行：

```sh
make
make run
```

Makefile 預設使用 `/usr/local/i386elfgcc/bin/i386-elf-gcc`、`i386-elf-ld`；工具鏈若在 PATH，可在 make 命令追加 `CC=i386-elf-gcc LD=i386-elf-ld OBJCOPY=i386-elf-objcopy`。

若只想啟動 VGA 視窗、不使用 Makefile 的音效參數：

```sh
qemu-system-i386 -m 128 \
  -drive file=os-image.bin,format=raw,if=floppy,index=0 \
  -display gtk,gl=off -rtc base=utc
```

此命令仍需要可用的圖形環境。沒有圖形桌面時，若 QEMU 編入 curses 支援，可改用 `-display curses`；畫面輸出走 VGA，單用 `-nographic` 不會自動變成 serial console。

## 五分鐘操作

| 輸入／動作 | 程式碼預期 | 回去讀哪裡 |
|---|---|---|
| `help` | 列出目前 shell 命令 | `user_shell_main` |
| `pid` | 顯示 shell 任務 PID | `SYS_GETPID`、`task_current` |
| `echo hello` | 印出 hello | `SYS_WRITE`、`kprint` |
| `touch note` | 印出 created | `fs_create` |
| `write note hello` | 印出 written | `fs_write` |
| `cat note` | 印出 hello | `fs_read` |
| `write note hi`，再 `cat note` | 只剩 hi，驗證覆蓋 | `file_table[idx].size` |
| `ls` | note 大小為 2 bytes | `fs_list` |
| `rm note`，再 `cat note` | deleted，接著 cat failed | `fs_delete` |
| 上下鍵 | 瀏覽 history | `navigate_history_up/down` |

另做一次 `write note hello`，關閉 QEMU 後重新啟動，再執行 `ls`：預期檔案表為空。這是在驗證資料沒有持久化。

`exit` 走 `task_exit`，目前沒有自動重啟 shell；重跑 QEMU 可重新開始。`time`、`calc`、`multitask` 等舊筆記指令不在目前 shell 解析表中。

## 在 GDB 看三個重要邊界

`make debug` 會啟動 QEMU 的 GDB server，但沒有 `-S`，CPU 不會等待 debugger。要從開機前設斷點，分別在兩個終端執行：

```sh
# 終端 A：建置並啟動，-S 暫停 CPU，-s 在 1234 提供 GDB server
make os-image.bin kernel.elf
qemu-system-i386 -m 128 -S -s \
  -drive file=os-image.bin,format=raw,if=floppy,index=0 \
  -display gtk,gl=off -rtc base=utc
```

```sh
# 終端 B
gdb kernel.elf
```

```gdb
set architecture i386
target remote localhost:1234
hbreak kernel_main
continue
info registers eip esp cs cr0 cr3
```

使用硬體 breakpoint 可避免在 BIOS 尚未載入核心前，把軟體 breakpoint 寫進尚未載入的 RAM。停在 `kernel_main` 後，核心已載入，可刪除此 breakpoint，再按目的加一般斷點：

```gdb
delete breakpoints
break enter_usermode
continue
info registers cs esp
```

此時還在 Ring 0；查看 `disassemble enter_usermode`，找 `iret`，用 `si` 逐指令觀察跨入 Ring 3 後 CS 的低兩位是否為 3。

另外一次練習可在 shell 啟動後觀察 syscall：

```gdb
break syscall_handler
continue
print /x r->eax
print /x r->cs
print /x r->ebx
```

shell 啟動本身也會 clear/write/read，所以第一次停下不一定是你手動輸入的命令。`r->cs` 是中斷前的 CS，debugger 當下的 `$cs` 則是執行 handler 的核心 CS。

觀察任務切換可設 `break context_switch`，對照 ASM 的保存順序與 `task_create` 的初始 stack；頻繁 timer breakpoint 會干擾互動，完成後刪除斷點。

## 自動回歸

```sh
make test
```

需要 Python 3、QEMU 與交叉工具鏈；shell 與排程測試使用本機 Unix QMP socket 注入鍵盤事件。各 QEMU 測試在暫存目錄編譯專用映像，不覆蓋平常啟動的映像。`make test` 包含 linker、user fault／syscall、阻塞／搶佔／睡眠、shell 與 kernel panic 五類檢查，詳見[測試矩陣](user-protection.md#自動驗證)。

## 關書自測

| 問題 | 答案線索 |
|---|---|
| 誰把核心放到 `0x8000`？誰假設它在那裡？ | `disk_load`／boot sector；`linker.ld` 與跳轉入口 |
| GDT、IDT、頁表分別管理什麼？ | segment／中斷入口／位址映射與存取權限 |
| `kmalloc(100)` 和配置一個 frame 有何不同？ | heap 的變長區塊／PMM 的 4 KiB 實體單位 |
| `ret` 與 `iret` 在此各出現在哪裡？ | 軟體 task context switch／中斷返回及進入 user mode |
| syscall 後一定換 PID 嗎？ | 不一定；切權限與排程是兩件事 |
| Ring 3 直接碰核心資料會怎樣？ | supervisor PTE 觸發 #PF；只終止故障 user task。其他 user 任務仍共享 user pages |
| Ring 3 的純計算迴圈如何被 timer 換走？ | IRQ frame 留在該 task 的核心 stack；schedule 更新 TSS 並切 ESP，日後由 iret 恢復 |
| shell 等鍵盤時，其他任務為何仍能跑？ | SYS_READ 只把呼叫者設為 BLOCKED；Enter 發布完整行後將它改為 READY |
| 有 RAM 檔案系統，是否就能從檔案執行程式？ | 還需要 executable loader 與對應的啟動／位址空間機制 |

## 文檔更新約定

更改開機或初始化時更新[架構](architecture.md)；更改 syscall、shell 或 I/O 時更新[追蹤流程](traces.md)與本頁；實作限制被修正後更新[概念頁](concepts.md)與首頁。歷史筆記保留作為舊版本學習資料，新現況寫進上述頁面。
