# 受保護的內建 Ring 3 shell

[回首頁](../README.md) · [原始計畫](next-feature-research.md) · [操作練習](practice.md)

本里程碑建立核心與使用者程式之間的頁面保護。所有任務仍共用同一份 page directory，user 任務可存取其他 user 任務的資料與 stack；不提供 per-process 隔離或 NX。後續已加入單 CPU 的 Ring 3 timer 搶佔、單讀者鍵盤等待與 timer 睡眠，詳見[排程實作與驗收](blocking-wakeup-preemption-plan.md)。

## 頁面與執行入口

[linker.ld](../linker.ld) 依 `user/*.o` 將 text、rodata、data、BSS 分區，user 區段邊界皆對齊 4096 bytes。前 8 MiB 保留 identity mapping，PTE 預設 supervisor；只開放 user 區段與存活 user stack。user text／rodata 的 RW=0，data／BSS／stack 的 RW=1。頁目錄與頁表的 identity、recursive 別名均為 supervisor；CR0.WP 讓核心寫入唯讀頁也會 fault。

user stack 由 PMM 配置完整 4 KiB frame，配置時清零、記入 PCB，再授予 user 權限。核心 stack 使用 8 KiB heap 區塊，容納 syscall 暫存資料。user object 中編譯器產生的 SSP 引用，透過 objcopy 重新命名至 [user/runtime.c](../user/runtime.c) 的專用 guard／failure handler，避免讀取核心 guard。

一般 IDT gate 為 DPL 0，只有 `0x80` 為 DPL 3。syscall／exception／IRQ stub 分別保存 DS／ES／FS／GS，切至 kernel selectors 並清除 DF 後才進 C，返回時恢復各 selector；user 的 DF 由 `iret` 恢復。user entry 使用固定 `EFLAGS=0x202`，由 TSS 提供進入核心時的 stack。

boot loader 目前讀取 128 個核心磁區，上限 65536 bytes；Makefile 同時將數量傳給 NASM 與用於映像大小檢查。跨越 BX 的 64 KiB 邊界時更新 ES。BSS 不占磁碟內容，啟動入口會在 constructors 前清零 user／kernel BSS。

## Syscall 契約

[cpu/usercopy.c](../cpu/usercopy.c) 集中檢查範圍溢位、每一頁的 PDE/PTE present 與 user 權限，輸出再檢查 PDE/PTE RW。字串逐位元組有界驗證，讀到 NUL 即停止。handler 先複製到核心 stack 的暫存區，再呼叫 console／FS；輸出先驗證完整宣告範圍，再複製實際產生的資料。

| 呼叫 | 指標契約 |
|---|---|
| `WRITE` | NUL 字串上限 1024 bytes，包含 NUL |
| `READ` | 驗證整個輸出容量可寫，才等待鍵盤；醒後複製時重新驗證；最多取一行 255 bytes，加 NUL |
| `FS_CREATE`、`FS_DELETE` | 名稱上限 32 bytes，包含 NUL |
| `FS_READ` | 有界複製名稱；驗證整個輸出容量可寫，最多複製檔案的 4096 bytes |
| `FS_WRITE` | 有界複製名稱；驗證整個輸入範圍，再複製最多 4096 bytes |

錯誤範圍回傳 `USER_EFAULT=-14`；上限內沒有 NUL 回傳 `USER_ENAMETOOLONG=-36`。名稱錯誤先於資料 buffer 錯誤；有效範圍但 FS write 超過 4096 bytes 回傳既有 `FS_ERR_OVERFLOW=-4`。buffer 長度是 uint32_t；零長度 FS buffer 不存取指標，仍需有效檔名；`READ` 零容量回傳 `-1`，已有另一位讀者等待時回傳 `SYS_EBUSY=-16`。`SLEEP` 秒數過大回傳 `-1`，零秒立即回傳 0。buffer 驗證與大小檢查先於 FS 查找，因此同時有多種錯誤時，可能先回傳範圍／大小錯誤，而非檔案不存在。

此複製機制依賴單 CPU、IRQ 關閉且複製期間頁映射不變。`READ` 阻塞目前 task 後其他 task 可以運作，所以醒來時以 `copy_to_user` 再次驗證映射。沒有可從 user 改變映射的 syscall。未實作通用 kernel page-fault fixup；核心自己的錯誤仍會 panic。

## 故障與任務回收

`exception_fault` 記錄 PID、vector、EIP、CR2（僅 #PF）及 error code。一般 user exception 結束當前任務；kernel exception 與 NMI／double fault／machine check 明確 panic，關閉 IRQ 並停在 halt 迴圈。

`task_exit` 在 IRQ 關閉時清除鍵盤等待者與睡眠狀態、標記終止並強制排程。scheduler 先選下一個存活任務、移除終止節點，再更新 current 與 TSS，最後切換 stack。其他任務恢復後或第一次進入 trampoline 時，才執行 `task_reap`：撤銷 user stack 權限、清零／釋放 frame、釋放 kernel stack，最後清空 PCB。entry 自然返回也走 `task_exit`。PID 0 保留為永不阻塞的 idle。

## 自動驗證

```sh
make test
```

需要 Python 3、NASM、i386-elf GCC/binutils 與 `qemu-system-i386`。工具鏈預設在 `/usr/local/i386elfgcc/bin/`；其他路徑可用 `make test CC=... LD=... OBJCOPY=... NM=...` 指定。shell 與排程測試需要建立本機 Unix QMP socket；受限沙箱可能需授權。四種 QEMU 測試各自在暫存目錄建立專用映像，不改變一般 `os-image.bin`。`PROTECTION_TEST`、`SCHED_TEST`、`PROTECTION_PANIC`、`DEBUG_CONSOLE` 只在對應測試編譯時開啟；正式映像不含故障注入行為。

| 測試 | 檢查內容 |
|---|---|
| [check_layout.py](../tests/check_layout.py) | 實際 ELF 的 user 邊界頁對齊、shell／kernel 位置、BSS 不超出 heap 上限 |
| [qemu_protection.py](../tests/qemu_protection.py) | 硬體頁表權限巡查、syscall matrix、4096-byte FS 讀寫、90 次故障與 survivor／timer 持續運作、PCB／stack 回收重用、kernel entry 返回 |
| [qemu_scheduling.py](../tests/qemu_scheduling.py) | 真實鍵盤 IRQ 的阻塞／喚醒、早到／空行／忙碌讀者／滿槽丟棄、pending IRQ 交界、醒後 user copy 失敗、waiter 退出與 PCB 重用、Ring 3 多輪搶佔與 TSS／frame、睡眠 deadline 順序／回繞與 user `SYS_SLEEP`、shell 與背景計算並行 |
| [qemu_shell.py](../tests/qemu_shell.py) | 透過 QMP 注入鍵盤，驗證 Ring 3 `help pid echo touch write cat ls rm` 與刪除後讀取失敗 |
| [qemu_panic.py](../tests/qemu_panic.py) | 核心寫入 user 唯讀 text，確認 #PF error=3、單次 panic 且未繼續啟動 shell |

syscall matrix 包含每個字串參數的 NULL、核心、未映射、recursive 頁表、高位址、跨入 supervisor 頁、未終止字串；`READ`／`FS_READ`／`FS_WRITE` buffer 包含核心／未映射／跨頁失敗及加長度溢位，輸出另測唯讀頁。測試也驗證合法跨頁 FS buffer、失敗寫入後檔案資料不變、user 設 DF 後仍可 syscall。

90 次 fault 循環包括直接讀／寫核心資料、頁目錄 identity 位址、兩個 recursive 位址、核心任務 stack／boot stack；寫 user text／rodata；`int 0x20` 的 #GP；`ud2` 的 #UD；讀／寫未映射位址的 #PF。每次比對 vector、error code、CR2（若為 #PF），確認 survivor 前進、核心 sentinel 不變，且死去 user stack 的 PTE 已撤銷 user 權限、內容已清零。循環數超過 32 個 PCB slot，測得回收重用。

這些是可重跑的具體檢查，不是對所有 x86 exception、任意 user 程式、SMP 或實體硬體的完整驗證。history／Tab 與不同 user 之間的隔離不在本次測試保證內。

## 保護階段的驗證與 review 結果

2026-10-01 執行 `make -s test`：layout、90 次 fault／syscall matrix、shell 鍵盤流程及 kernel panic 全部通過；`git diff --check` 通過。一般映像的 `kernel.bin` 為 36864 bytes，小於 65536-byte 載入上限。

三個 subagent 分別審查任務生命週期、頁面／syscall／build 安全性，以及文件與實作一致性。已處理其建議：補測最大檔案、未映射 fault、kernel panic、stack 撤權／清零、entry 返回與零長度／超限 buffer；補齊 NASM include dependencies、可配置 NM，以及錯誤優先序說明。最終未留下阻擋問題；上述共享 user address space 等限制仍明確保留。
