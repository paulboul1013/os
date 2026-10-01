# 概念與實作對照

[回首頁](../README.md) · [架構](architecture.md) · [追蹤流程](traces.md) · [練習](practice.md)

每一節先回答 OS 要解決什麼問題，再指出這個 repo 用哪些資料結構與函式實作。建議每節花五分鐘，開著連結中的程式一起讀。

## 1. 開機：誰先讓 C 程式跑起來？

C 核心需要程式碼已在正確位址、可用的 stack，以及正確的 CPU 模式。這些由 boot loader 和組語入口先準備；linker 決定的位址必須與 loader 放置程式碼的位置一致。

**讀碼順序：** [boot/bootsect.asm](../boot/bootsect.asm) 的 `load_kernel` → [boot/switch_pm.asm](../boot/switch_pm.asm) 的 `switch_to_pm` → [boot/kernel_entry.asm](../boot/kernel_entry.asm) 的 `_start` → [kernel/kernel.c](../kernel/kernel.c)。

記住 `CR0.PE` 是進 protected mode，`CR0.PG` 才是開 paging。`lgdt` 載入描述符表；far jump 更新 CS。開 paging 不是進入 32 位元模式的必要條件。

**自問：** 核心改連結到另一個位址，為什麼不能只改 linker script？因為磁碟載入目的地與跳轉入口也必須一致。

## 2. 中斷：CPU 怎麼回應外部事件？

中斷讓 CPU 暫停原本的指令流，執行處理常式，再恢復現場。例外由執行指令引發，IRQ 由裝置引發，`int 0x80` 則是程式主動執行的軟體中斷。

**讀碼順序：** [cpu/isr.c](../cpu/isr.c) 的 `isr_install` → [cpu/idt.c](../cpu/idt.c) → [cpu/interrupt.asm](../cpu/interrupt.asm) → `irq_handler` → 裝置 callback。

| 元件 | 回答的問題 | 本 repo 的例子 |
|---|---|---|
| IDT | 中斷向量要跳到哪裡？ | `set_idt_gate` 設定入口 |
| PIC | 裝置 IRQ 對應哪個向量？ | IRQ0–15 重映射到 32–47 |
| ASM stub | 怎麼把暫存器交給 C？ | 保存暫存器，傳入 `registers_t *` |
| handler table | 哪個驅動處理這個事件？ | `register_interrupt_handler` |
| EOI | 告知 PIC 此次中斷已處理 | `irq_handler` 在 callback 前發送 |
| `iret` | 怎麼恢復中斷前的執行？ | 恢復 EIP、CS、EFLAGS；跨權限返回時另恢復 ESP、SS |

對照 [cpu/isr.h](../cpu/isr.h) 的 `registers_t` 與 ASM 的 push 順序；跨權限切換時才有 CPU 保存的 user ESP/SS，不要假設所有中斷 frame 都一樣長。

**自問：** PIC 的 IRQ 編號為什麼不直接等於 IDT 向量？因為前面的向量留給 CPU exceptions，重映射後 IRQ0 在 32。

## 3. 記憶體：配置與映射各做什麼？

實體頁框配置決定哪塊 RAM 已被使用；分頁決定虛擬位址如何轉成實體位址與是否允許存取；heap 則把一段可用空間切成程式要求的大小。

| 層 | 單位／資料結構 | 本 repo 的函式與限制 |
|---|---|---|
| PMM | 4 KiB frame、bitmap | [pmm_alloc_frame](../cpu/pmm.c)：掃描第一個空閒 bit；沒有解析硬體 memory map |
| Paging | page directory、page table、PTE | [init_paging](../cpu/paging.c)：前 8 MiB VA=PA，CR3 指向頁目錄 |
| Heap | `header_t` 串列、可變長區塊 | [kmalloc / kfree](../libc/mem.c)：first fit、切割、合併相鄰空閒區塊 |

32-bit、4 KiB 分頁可將位址理解成 `頁目錄索引 10 bits | 頁表索引 10 bits | 頁內位移 12 bits`。一張 1024-entry 頁表涵蓋 4 MiB，因此此處用兩張頁表覆蓋 8 MiB。

heap 使用 `.bss` 結尾之後到 `0x80000` 的固定區域；PMM 保留低 1 MiB，頁表由更高位址的 frame 配置。`kmalloc` 不會在 heap 用完時向 PMM 追加頁面。

**自問：** `pmm_alloc_frame()` 成功就能直接把回傳值轉指標使用嗎？只在該實體位址有合適映射時成立；目前 identity mapping 的範圍有限。

## 4. 任務：如何暫停 A、接著跑 B？

PCB 保存任務管理資訊，stack 保存執行中的呼叫與暫存器。排程器選任務，context switch 改變 CPU 執行現場；兩者責任不同。

**讀碼順序：** [cpu/task.h](../cpu/task.h) 的 `pcb_t` → [cpu/task.c](../cpu/task.c) 的 `task_create` → [cpu/scheduler.c](../cpu/scheduler.c) 的 `schedule` → [cpu/context_switch.asm](../cpu/context_switch.asm)。

`task_create` 配置 8 KiB 核心 stack（包含 syscall 暫存 buffer），預先放入 trampoline address、EBP、EBX、ESI、EDI、EFLAGS。從保存的 ESP 往高位址讀，順序是：

```text
ESP → EFLAGS → EDI → ESI → EBX → EBP → task_start／返回位址
```

`context_switch(&old->esp, next->esp)` 保存舊 ESP、載入新 ESP，pop 暫存器後 `ret`。新任務第一次 `ret` 跳到 `task_start`，回收已終止任務後開啟中斷，再呼叫 entry；entry 返回會自動 `task_exit`；舊任務恢復時則返回先前呼叫位置。中斷途徑另外由 IRQ stub 保存其他暫存器。

Round-Robin 沿串列找下一個 READY 任務，到尾端回到 head。共有 32 個 PCB slot，task 0 是原本的核心執行。`TASK_BLOCKED` 已定義，但沒有完整等待／喚醒機制。

**自問：** 為何這裡的任務不像各有獨立位址空間的行程？PCB 沒有每任務頁目錄，context switch 也沒有替任務切 CR3。

## 5. 特權與 syscall：shell 如何請核心幫忙？

Ring 3 限制可執行的特權操作。系統呼叫提供受控入口，讓使用者程式提出請求；CPU 經 IDT 進 Ring 0，核心處理後用 `iret` 返回。

**讀碼順序：** [cpu/gdt.c](../cpu/gdt.c) → [cpu/tss.c](../cpu/tss.c) → [cpu/usermode.c](../cpu/usermode.c) → [cpu/syscall.asm](../cpu/syscall.asm) → [kernel/syscall.c](../kernel/syscall.c)。

| 值 | 角色 |
|---|---|
| `0x08`、`0x10` | kernel code、data selector |
| `0x18` | TSS selector，由 `ltr` 載入 |
| `0x23`、`0x2b` | user code、data selector，包含 RPL=3 |
| TSS `ss0:esp0` | 從 Ring 3 進 Ring 0 時要使用的核心 stack |

`enter_usermode` 準備假的返回 frame，讓 `iret` 進入 user entry。TSS 在這裡提供跨權限的 stack；任務切換本身用軟體 `context_switch`。切 Ring 可仍是同一個任務，不能把每次 syscall 都當成排程。

syscall 約定：EAX 是編號與回傳值，EBX／ECX／EDX 帶參數。這是本專案自己的 ABI，不應當作 Linux syscall ABI 使用。

**自問：** 為什麼 shell 呼叫 `sys_write` 沒有直接 `call kprint`？核心頁面是 supervisor，直接 call 會 fault。`int 0x80` 是唯一 DPL 3 gate；handler 檢查並複製 user 指標，避免替 user 存取核心資料。

## 6. I/O 與檔案：裝置資料怎麼成為程式可用的內容？

[keyboard_callback](../drivers/keyboard.c) 從 port `0x60` 讀 scancode，處理字元、游標、history，Enter 後將一行放進共享 buffer。`SYS_READ` 把完成的一行複製到 shell。輸出則由 [screen.c](../drivers/screen.c) 寫入 VGA 文字記憶體。

[SimpleFS](../fs/fs.c) 用 `file_table[64]` 保存每個檔案的名稱、最多 4096 bytes 資料、大小與使用旗標。名稱最多 31 bytes，另外保留 NUL。查找是線性掃描；write 覆蓋整個內容，read 從開頭讀到 buffer 上限。

目前沒有目錄、file descriptor、每次開啟的 offset 或磁碟持久化。`fs_append`、`fs_stat`、`fs_rename` 有核心 API，但未透過目前 syscall／shell 公開。

**自問：** boot loader 會讀磁碟，為什麼 `write note hello` 重開機後仍消失？boot 讀取核心映像與執行期檔案儲存是兩條不同功能路徑；SimpleFS 沒有寫磁碟。

## 實作界線與後續閱讀題目

本次已建立核心與 user 的保護邊界，仍保留下列教學簡化。

| 觀察 | 證據與影響 |
|---|---|
| user 任務共用頁目錄 | user data 與所有存活 user stack 對各 user 任務可見；沒有 per-process 隔離 |
| 沒有 NX 保護 | 使用傳統 32-bit paging，user 可寫頁仍可執行；text／rodata 以 RW=0 保護 |
| 使用者程式不受 timer 直接搶佔 | `scheduler_timer_handler` 遇到 `regs->cs & 3 != 0` 就返回 |
| 讀取輸入停用全域排程 | `SYS_READ` 以 `scheduler_disable` 與 `sti; hlt; cli` 等待，尚無每任務等待佇列 |
| heap 的 align 參數未真正完成 | `kmalloc` 計算 padding，但沒有調整回傳位置；kernel stack 不要求頁對齊，user stack 改用 PMM 完整頁 |
| PMM 未解析硬體 memory map | 假設 128 MiB RAM；目前僅映射前 8 MiB，user stack 與頁表配置會檢查此範圍 |
| Tab 命令表與 shell 不一致 | `available_commands` 還有 `end/page/calc/time`；以 `user_shell_main` 解析分支為現況 |

已修正的保護機制包括：PTE 的硬體 bit 位置、啟動時清空 BSS、只配置一頁的 page directory、supervisor 頁表別名、CR0.WP、一般 IDT gate DPL 0、syscall 完整範圍驗證與有界字串複製。任務結束會先移出 queue，切換 stack 後撤銷 user stack 權限、清零／釋放頁框、釋放 kernel stack，最後清空 PCB 供重用。user fault 記錄後走相同回收流程；kernel fault 進入停機迴圈。

測試與錯誤回傳契約見[保護設計與驗證](user-protection.md)。這些限制不能直接沿用成通用 OS 的設計保證。
