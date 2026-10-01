# 下一個功能：讓 Ring 3 shell 具備真正的記憶體保護

研究日期：2026-09-30。以下保留研究當時的原始碼觀察與計畫基線。

實作更新：第一個里程碑已實作；頁權限、syscall 複製、user stack／SSP、IDT 入口及故障回收的設計與自動測試見[保護設計與驗證](user-protection.md)。不同 user 任務之間的隔離與阻塞／喚醒仍屬後續範圍。

## 建議與理由

**優先實作「受保護的使用者程式執行環境」：分開核心與內建 shell 的頁面權限，檢查 syscall 指標，並讓使用者程式故障後核心仍能運作。** 這是依本 repo 現況做的優先序判斷，並非 OSDev 規定的開發順序。

目前已有 Ring 3、`int 0x80`、排程器與 RAM 檔案操作，適合把「權限切換」補成實際可驗證的「保護邊界」。新增 ELF 程式或更多檔案操作之前，先確保一般程式的錯誤不會改壞核心。

| 原始碼觀察 | 意義 |
| --- | --- |
| [paging.c](../cpu/paging.c) 的 `init_paging()` 把前 8 MiB 全部設成 `user=1`、`rw=1`，PDE 使用 `0x7` | 包含核心、頁表及核心堆疊的映射也能被 Ring 3 讀寫；進入 Ring 3 不等於已隔離核心 |
| [syscall.c](../kernel/syscall.c) 把暫存器直接轉成字串／buffer 指標 | 即使關閉核心頁面的 user 權限，核心仍可能透過 syscall 代使用者讀寫核心記憶體 |
| [idt.c](../cpu/idt.c) 的所有 gate 使用 `0xEE` | 一般中斷入口也開放 Ring 3 使用 `INT` 觸發，需要縮小可呼叫入口 |
| [paging.c](../cpu/paging.c) 的 page fault handler 印訊息後 `hlt` | 尚無隔離故障任務並恢復其他工作的流程 |

OSDev 的 [Setting Up Paging](https://wiki.osdev.org/Setting_Up_Paging) 示範 supervisor 映射；[X86 Paging](https://wiki.osdev.org/Paging) 說明 user page 的 PDE 與 PTE 都必須允許 user 存取。該頁帶有事實準確性爭議標記，因此實作硬體細節時仍應對照處理器手冊。[System Calls](https://wiki.osdev.org/System_Calls#Security/safety_implications) 明確要求檢查參數範圍及使用者指標。[IDT](https://wiki.osdev.org/Interrupt_Descriptor_Table) 說明 DPL 限制 `INT` 指令的使用，硬體中斷不受這項限制。

## 第一個里程碑的範圍

先沿用一份 page directory 與內建 shell，完成核心／使用者隔離；**這一階段不宣稱不同使用者任務之間已隔離。**

1. **分開頁面配置。** 修改 [linker.ld](../linker.ld) 與 [Makefile](../Makefile)，把 user 程式碼、唯讀資料、可寫資料分成獨立且以頁面對齊的區段。核心頁面設 supervisor；user text／rodata 唯讀，user data／stack 可寫。頁表本身與其所有別名也必須受到保護。
2. **獨立 user stack 與 runtime。** [usermode.c](../cpu/usermode.c) 現在由核心 heap 配置未對齊的 user stack，不能直接把所在頁整頁開放。改用專用完整頁面；同時處理 [ssp.c](../libc/ssp.c) 的共享 stack protector 符號，避免使用者程式依賴受保護的核心 guard／failure handler。
3. **建立安全的 syscall 資料複製介面。** 以 `copy_from_user`／`copy_to_user` 類型介面集中檢查：位址加長度溢位、整個範圍的每一頁、PDE/PTE user 權限、輸出 buffer 可寫性。字串必須有長度上限，不能只檢查第一個位址；所有 console 與檔案 syscall 都適用。
4. **限定核心入口。** 一般 IDT gate 用 DPL 0，`0x80` 用 DPL 3；依設計另行開放的除錯 gate 才是例外。
5. **可控地結束故障任務。** 區分 user fault 與 kernel fault，記錄 PID、EIP、CR2／error code，user fault 結束該任務並回到存活任務／idle。先修正 [task.c](../cpu/task.c) 目前在自己 stack 上 `kfree` 後才切換的順序，以延後回收方式處理，並處理 terminated 任務移出排程鏈及 PCB 重用。核心故障仍應明確 panic，不能當成普通 user fault 忽略。

配置調整會增加映像大小；[Makefile](../Makefile) 目前只允許 64 個核心磁區，頁面對齊後需重新檢查映像大小與 boot loader 載入範圍。這一里程碑不需要 higher-half kernel、ELF loader、`fork` 或磁碟驅動。

## 完成條件

- 原有 `help`、`pid`、`echo`、`touch`、`write`、`cat`、`ls`、`rm` 仍可從 Ring 3 操作。
- 測試 user 任務直接讀／寫核心記憶體、頁表、核心 stack，或寫入 user 唯讀頁時產生預期 fault；只結束該任務，核心與另一個測試任務仍可繼續。
- 每個接收指標的 syscall 測試：核心位址、未映射位址、跨頁範圍、位址溢位、未終止字串、唯讀輸出 buffer；應回傳約定錯誤，核心資料不能改變。
- Ring 3 `int 0x80` 正常；嘗試一般 DPL 0 gate 時產生可控的 #GP；鍵盤與 timer 硬體 IRQ 仍正常。
- 多次建立／終止故障任務後，排程鏈與 stack 回收仍正常。驗證頁表與 linker map，確認沒有意外開放的核心頁面。

以上保留原訂驗收要求；目前可重跑的測試、實際涵蓋範圍與剩餘限制見[自動驗證](user-protection.md#自動驗證)。

## 其他候選與後續順序

| 候選 | 現況與判斷 |
| --- | --- |
| 阻塞／喚醒、使用者搶佔 | 是緊接著該做的功能。[syscall.c](../kernel/syscall.c) 等待輸入時停用整個 scheduler；[scheduler.c](../cpu/scheduler.c) 遇到 Ring 3 timer frame 直接返回；[task.h](../cpu/task.h) 的 `TASK_BLOCKED` 尚未形成等待機制。先做這項的可見成果是「shell 等鍵盤時背景任務仍能跑」，但它不解決記憶體保護。 |
| 每個 process 的 address space 與 ELF `spawn` | 在保護與排程基礎穩定後做。現有 [PCB](../cpu/task.h) 沒有 page directory 欄位，user 程式也仍和 kernel 一起連結；載入器還需要定義映射、stack、entry point 與生命週期。 |
| 磁碟持久化 | [SimpleFS](../fs/fs.c) 目前是靜態 RAM 檔案表。若需求改成「重開機保留資料」，這項可優先；以學習 OS 核心機制為目標，目前保護與排程更值得先完成。 |

[Blocking Process](https://wiki.osdev.org/Blocking_Process) 把等待描述為任務暫時不參與執行，直到事件發生；[Brendan's Multi-tasking Tutorial](https://wiki.osdev.org/Brendan%27s_Multi-tasking_Tutorial) 的 Step 6 提供 block／unblock 流程，適合下一階段。實作時必須處理「檢查條件到進入等待之間」的 lost wakeup，並保持 idle 可執行；安全的 Ring 3 搶佔還需核對中斷 frame、TSS 與重入時機。[ELF](https://wiki.osdev.org/ELF) 則可作為後續 program header／segment 載入的參考。

建議順序：**受保護的內建 shell → 阻塞／喚醒與安全的使用者搶佔 → 獨立 process address space 與 ELF 執行 → 磁碟持久化。**
