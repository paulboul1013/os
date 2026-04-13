#include "fs.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../libc/stdio.h"
#include "../drivers/screen.h"

/* ============================================================
 *  內部資料
 * ============================================================ */

/* 靜態配置的檔案表 — 所有檔案都在此陣列中管理 */
static fs_file_t file_table[FS_MAX_FILES];

/* ============================================================
 *  內部工具函式
 * ============================================================ */

/* 用名稱搜尋檔案，回傳索引；找不到回傳 -1 */
static int fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].in_use && strcmp((char*)file_table[i].name, (char*)name) == 0) {
            return i;
        }
    }
    return -1;
}

/* 找到一個空閒的檔案 slot，回傳索引；沒有空間回傳 -1 */
static int fs_find_free(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

/* 安全的字串複製 (類似 strncpy) */
static void fs_strncpy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* ============================================================
 *  API 實作
 * ============================================================ */

/*
 * fs_init — 初始化檔案系統
 * 清空整個檔案表
 */
void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        file_table[i].in_use = 0;
        file_table[i].size = 0;
        file_table[i].name[0] = '\0';
    }
    kprint("[FS] SimpleFS initialized (max ");
    char num[8];
    int_to_ascii(FS_MAX_FILES, num);
    kprint(num);
    kprint(" files, ");
    int_to_ascii(FS_MAX_FILESIZE, num);
    kprint(num);
    kprint(" bytes/file)\n");
}

/*
 * fs_create — 建立一個空白檔案
 *
 * @param name  檔案名稱
 * @return      FS_OK 成功, FS_ERR_EXISTS 已存在,
 *              FS_ERR_FULL 空間已滿, FS_ERR_INVALID 名稱無效
 */
int fs_create(const char *name) {
    if (name == 0 || name[0] == '\0') return FS_ERR_INVALID;
    if (strlen(name) >= FS_MAX_FILENAME) return FS_ERR_INVALID;
    if (fs_find(name) >= 0) return FS_ERR_EXISTS;

    int idx = fs_find_free();
    if (idx < 0) return FS_ERR_FULL;

    fs_strncpy(file_table[idx].name, name, FS_MAX_FILENAME);
    file_table[idx].size = 0;
    file_table[idx].in_use = 1;

    /* 清空資料區 */
    memory_set(file_table[idx].data, 0, FS_MAX_FILESIZE);

    return FS_OK;
}

/*
 * fs_read — 讀取檔案內容
 *
 * @param name      檔案名稱
 * @param buf       存放讀取資料的 buffer
 * @param buf_size  buffer 大小
 * @return          實際讀取位元組數, 或負值表示錯誤
 */
int fs_read(const char *name, uint8_t *buf, uint32_t buf_size) {
    int idx = fs_find(name);
    if (idx < 0) return FS_ERR_NOT_FOUND;

    uint32_t to_read = file_table[idx].size;
    if (to_read > buf_size) to_read = buf_size;

    memory_copy(file_table[idx].data, buf, (int)to_read);
    return (int)to_read;
}

/*
 * fs_write — 寫入資料到檔案 (覆蓋)
 *
 * @param name  檔案名稱
 * @param data  要寫入的資料
 * @param len   寫入長度
 * @return      FS_OK 成功, FS_ERR_NOT_FOUND 找不到,
 *              FS_ERR_OVERFLOW 超出容量
 */
int fs_write(const char *name, const uint8_t *data, uint32_t len) {
    int idx = fs_find(name);
    if (idx < 0) return FS_ERR_NOT_FOUND;
    if (len > FS_MAX_FILESIZE) return FS_ERR_OVERFLOW;

    memory_copy((uint8_t*)data, file_table[idx].data, (int)len);
    file_table[idx].size = len;
    return FS_OK;
}

/*
 * fs_append — 附加資料到檔案末尾
 *
 * @param name  檔案名稱
 * @param data  要附加的資料
 * @param len   附加長度
 * @return      FS_OK 成功, FS_ERR_NOT_FOUND 找不到,
 *              FS_ERR_OVERFLOW 超出容量
 */
int fs_append(const char *name, const uint8_t *data, uint32_t len) {
    int idx = fs_find(name);
    if (idx < 0) return FS_ERR_NOT_FOUND;
    if (file_table[idx].size + len > FS_MAX_FILESIZE) return FS_ERR_OVERFLOW;

    memory_copy((uint8_t*)data,
                file_table[idx].data + file_table[idx].size,
                (int)len);
    file_table[idx].size += len;
    return FS_OK;
}

/*
 * fs_delete — 刪除指定檔案
 *
 * @param name  檔案名稱
 * @return      FS_OK 成功, FS_ERR_NOT_FOUND 找不到
 */
int fs_delete(const char *name) {
    int idx = fs_find(name);
    if (idx < 0) return FS_ERR_NOT_FOUND;

    file_table[idx].in_use = 0;
    file_table[idx].size = 0;
    file_table[idx].name[0] = '\0';
    memory_set(file_table[idx].data, 0, FS_MAX_FILESIZE);

    return FS_OK;
}

/*
 * fs_list — 列出所有檔案 (名稱 + 大小)
 */
void fs_list(void) {
    int count = 0;
    kprint("--- SimpleFS File List ---\n");
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            char size_str[12];
            int_to_ascii((int)file_table[i].size, size_str);
            printf("  [%d] %s  (%s bytes)\n", i, file_table[i].name, size_str);
            count++;
        }
    }
    if (count == 0) {
        kprint("  (empty - no files)\n");
    } else {
        char cnt_str[8];
        int_to_ascii(count, cnt_str);
        printf("Total: %s file(s)\n", cnt_str);
    }
}

/*
 * fs_stat — 顯示檔案資訊
 *
 * @param name  檔案名稱
 * @return      FS_OK 成功, FS_ERR_NOT_FOUND 找不到
 */
int fs_stat(const char *name) {
    int idx = fs_find(name);
    if (idx < 0) return FS_ERR_NOT_FOUND;

    printf("File: %s\n", file_table[idx].name);
    printf("Size: %d bytes\n", file_table[idx].size);
    printf("Max:  %d bytes\n", FS_MAX_FILESIZE);
    printf("Slot: %d\n", idx);

    return FS_OK;
}

/*
 * fs_rename — 重新命名檔案
 *
 * @param old_name  原檔名
 * @param new_name  新檔名
 * @return          FS_OK 成功, FS_ERR_NOT_FOUND 原檔不存在,
 *                  FS_ERR_EXISTS 新名稱已被使用, FS_ERR_INVALID 名稱無效
 */
int fs_rename(const char *old_name, const char *new_name) {
    if (new_name == 0 || new_name[0] == '\0') return FS_ERR_INVALID;
    if (strlen(new_name) >= FS_MAX_FILENAME) return FS_ERR_INVALID;

    int idx = fs_find(old_name);
    if (idx < 0) return FS_ERR_NOT_FOUND;

    /* 確保新名稱沒有被使用 */
    if (fs_find(new_name) >= 0) return FS_ERR_EXISTS;

    fs_strncpy(file_table[idx].name, new_name, FS_MAX_FILENAME);
    return FS_OK;
}
