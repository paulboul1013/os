# OS

這是一個以 C 與 NASM 組合語言實作的 **32 位元 x86 教學作業系統**。自製 boot sector 經 BIOS 載入核心，進入 protected mode，建立中斷、分頁與任務系統，最後啟動 Ring 3 shell。核心服務集中在同一個核心中，可視為小型單體核心。

## 怎麼讀

| 時間／目的 | 閱讀路線 |
|---|---|
| 5 分鐘回想全貌 | 本頁 → [架構與開機流程](docs/architecture.md) |
| 30 分鐘複習概念＋程式 | [概念與實作對照](docs/concepts.md)，依序讀六個主題 |
| 跟著一次操作理解 OS | [從 shell 追到硬體與核心](docs/traces.md) |
| 理解這次 Ring 3 保護更新 | [從 shell 看懂保護、syscall 與排程](docs/protection-walkthrough.md) |
| 自己跑、設斷點、回答問題 | [操作與複習練習](docs/practice.md) |
| 查以前的推導與開發紀錄 | [原始學習筆記](docs/legacy-notes.md)（歷史資料） |

建議第一個開啟的程式是 [kernel/kernel.c](kernel/kernel.c) 的 `kernel_main()`：它是各子系統接在一起的地方。

## 一張表記住目前做到哪裡

以下依目前原始碼整理；「接上」表示有呼叫路徑，不表示已完成所有正確性與隔離驗證。

| 子系統 | 現況 | 程式入口 |
|---|---|---|
| 開機 | BIOS、軟碟 CHS 讀取、自製 GDT、32-bit protected mode | [boot/bootsect.asm](boot/bootsect.asm) |
| 中斷／裝置 | IDT、PIC、50 Hz PIT、鍵盤 IRQ、VGA 文字輸出 | [cpu/isr.c](cpu/isr.c)、[drivers/](drivers/) |
| 記憶體 | bitmap PMM、核心 heap、前 8 MiB identity mapping、核心／user 頁權限 | [cpu/pmm.c](cpu/pmm.c)、[cpu/paging.c](cpu/paging.c)、[libc/mem.c](libc/mem.c) |
| 任務 | PCB、Round-Robin、以 ESP 切換核心堆疊 | [cpu/scheduler.c](cpu/scheduler.c)、[cpu/context_switch.asm](cpu/context_switch.asm) |
| 使用者模式 | GDT user segments、TSS、獨立 user stack、故障隔離 | [cpu/usermode.c](cpu/usermode.c) |
| 系統呼叫 | `int 0x80`，編號 0–11 | [kernel/syscall.c](kernel/syscall.c) |
| Shell | `help clear pid echo ls touch cat write rm exit`，另有 `?` | [user/shell.c](user/shell.c) |
| 檔案 | SimpleFS，64 個檔案，每檔最多 4096 bytes；重開機消失 | [fs/fs.c](fs/fs.c) |

**目前的保護界線：** 核心頁面與頁表為 supervisor，user text／rodata 唯讀，syscall 透過檢查與複製存取 user buffer；user fault 會結束該任務並延後回收 stack。所有任務仍共用頁目錄，**不同 user 任務之間尚未隔離**；timer 遇到 Ring 3 時不切換，`SYS_READ` 等輸入時仍停用全域排程器。詳見[保護設計與驗證](docs/user-protection.md)及[實作界線](docs/concepts.md#實作界線與後續閱讀題目)。

## 建置與執行

需要 `make`、NASM、i386-elf GCC/binutils 與 `qemu-system-i386`。Makefile 預設交叉工具鏈在 `/usr/local/i386elfgcc/bin/`。

```sh
make
make run
```

若工具鏈已放在 PATH：

```sh
make CC=i386-elf-gcc LD=i386-elf-ld OBJCOPY=i386-elf-objcopy
make run CC=i386-elf-gcc LD=i386-elf-ld OBJCOPY=i386-elf-objcopy
```

`make run` 預設使用 GTK/X11 與 PulseAudio。環境不適合時，見[不含音效的啟動方式與 GDB](docs/practice.md)。

可執行 `make test` 驗證 linker 配置、QEMU 保護測試、shell 鍵盤流程與核心 panic。測試需求與涵蓋範圍見[驗證說明](docs/user-protection.md#自動驗證)。

## 文檔依據

這組文檔以 repo 的 C、ASM、標頭、Makefile 與 linker script 為依據，將一般概念和本專案實作分開描述。原 README 完整保留於歷史筆記；其中舊 shell 指令、程式片段與待辦狀態不再作為現況說明。
