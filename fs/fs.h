#ifndef FS_H
#define FS_H

#include <stdint.h>

/*
 * Simple In-Memory File System (SimpleFS)
 *
 * 支援的操作 (CRUD):
 *   Create  - fs_create(name)         建立檔案
 *   Read    - fs_read(name, buf, sz)  讀取檔案內容
 *   Update  - fs_write(name, data, len) 寫入/覆蓋檔案內容
 *            fs_append(name, data, len) 附加內容到檔案末尾
 *   Delete  - fs_delete(name)         刪除檔案
 *
 * 其他:
 *   fs_list()                          列出所有檔案
 *   fs_stat(name)                      顯示檔案資訊
 *   fs_rename(old, new)                重新命名檔案
 */

/* 配置常數 */
#define FS_MAX_FILES      64        /* 最多可建立的檔案數量 */
#define FS_MAX_FILENAME   32        /* 檔案名稱最大長度 */
#define FS_MAX_FILESIZE   4096      /* 單一檔案最大容量 (4KB) */

/* 錯誤碼 */
#define FS_OK             0
#define FS_ERR_NOT_FOUND  -1       /* 檔案不存在 */
#define FS_ERR_EXISTS     -2       /* 檔案已存在 */
#define FS_ERR_FULL       -3       /* 檔案系統已滿 */
#define FS_ERR_OVERFLOW   -4       /* 資料超過檔案容量上限 */
#define FS_ERR_INVALID    -5       /* 無效的參數 */

/* 檔案節點結構 */
typedef struct {
    char     name[FS_MAX_FILENAME];      /* 檔案名稱 */
    uint8_t  data[FS_MAX_FILESIZE];      /* 檔案資料 */
    uint32_t size;                        /* 目前資料大小 (bytes) */
    uint8_t  in_use;                      /* 是否正在使用 */
} fs_file_t;

/* ============================================================
 *  API 函式
 * ============================================================ */

/* 初始化檔案系統 */
void fs_init(void);

/* Create — 建立一個空白檔案 */
int fs_create(const char *name);

/* Read — 讀取檔案內容到 buffer，回傳實際讀取的位元組數，失敗回傳負值 */
int fs_read(const char *name, uint8_t *buf, uint32_t buf_size);

/* Update — 寫入資料到檔案 (覆蓋原有內容) */
int fs_write(const char *name, const uint8_t *data, uint32_t len);

/* Update — 附加資料到檔案末尾 */
int fs_append(const char *name, const uint8_t *data, uint32_t len);

/* Delete — 刪除指定檔案 */
int fs_delete(const char *name);

/* 列出所有檔案 */
void fs_list(void);

/* 顯示指定檔案的資訊 (名稱、大小) */
int fs_stat(const char *name);

/* 重新命名檔案 */
int fs_rename(const char *old_name, const char *new_name);

#endif
