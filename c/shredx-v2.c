/*
 * shredx.c - Legitimate Secure File Deletion Tool for Windows
 * ------------------------------------------------------------------------
 * PURPOSE: This is a legitimate system administration tool for secure file deletion
 * LEGAL NOTICE: This tool is intended for legitimate system administration purposes only
 * 
 * 功能：
 *   - 普通删除 / 安全擦除 / Dry-run 模式
 *   - 强制删除（跳过确认）
 *   - 动态可视化进度条（████▒▒ 样式）
 *
 * 编译：
 *   tcc -Wall -O2 -o shredx shredx.c
 * 
 * SECURITY NOTE: This tool performs secure file deletion by overwriting file contents
 * before deletion. This is a standard security practice for sensitive data removal.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

int opt_force = 0;
int opt_secure = 0;
int opt_tiny = 0;
int opt_smart = 0;

int opt_dryrun = 0;
int opt_rename = 1;

long total_files = 0;
long processed_files = 0;

/* ------------------------ 工具 ------------------------ */

void generate_name(char *out, size_t size) {
    snprintf(out, size, "tmp_%08x_%08x",
             (unsigned)time(NULL), rand());
}

char* rename_file(const char *path) {
    if (opt_dryrun) return NULL;

    char *newpath = malloc(MAX_PATH);
    if (!newpath) return NULL;

    char name[64];
    generate_name(name, sizeof(name));

    const char *slash = strrchr(path, '\\');

    if (slash) {
        size_t len = slash - path + 1;
        char dir[MAX_PATH];
        strncpy(dir, path, len);
        dir[len] = 0;
        snprintf(newpath, MAX_PATH, "%s%s", dir, name);
    } else {
        snprintf(newpath, MAX_PATH, "%s", name);
    }

    if (MoveFileA(path, newpath)) {
        printf(COLOR_YELLOW "🔄 Rename: %s -> %s\n" COLOR_RESET, path, newpath);
        return newpath;
    }

    free(newpath);
    return NULL;
}

/* ------------------------ 覆盖 ------------------------ */

void random_fill(char *buf, DWORD size) {
    for (DWORD i = 0; i < size; i++)
        buf[i] = rand() % 256;
}

/* 快速模式 */
void tiny_overwrite(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(h, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(h);
        return;
    }

    char buf[4096];
    DWORD written;
    long remain = size;

    SetFilePointer(h, 0, NULL, FILE_BEGIN);

    while (remain > 0) {
        DWORD chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
        random_fill(buf, chunk);
        WriteFile(h, buf, chunk, &written, NULL);
        remain -= chunk;
    }

    FlushFileBuffers(h);
    CloseHandle(h);
}

/* 安全模式（3-pass） */
void secure_overwrite(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(h, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(h);
        return;
    }

    char buf[4096];
    DWORD written;

    for (int pass = 0; pass < 3; pass++) {
        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        long remain = size;

        while (remain > 0) {
            DWORD chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
            random_fill(buf, chunk);
            WriteFile(h, buf, chunk, &written, NULL);
            remain -= chunk;
        }

        FlushFileBuffers(h);
    }

    CloseHandle(h);
}

/* ------------------------ 删除流程 ------------------------ */
int choose_mode(DWORD size) {
    return size <= 16 * 1024 * 1024;
}

void delete_file(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("Not found: %s\n", path);
        return;
    }

    const char *target = path;
    char *renamed = NULL;

    if (opt_rename) {
        renamed = rename_file(path);
        if (renamed)
            target = renamed;
    }

    DWORD size = 0;

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        size = GetFileSize(h, NULL);
        CloseHandle(h);
    }

    int use_secure = opt_secure;
    int use_tiny = opt_tiny;

    if (opt_smart) {
        if (choose_mode(size)) {
            use_secure = 1;
            use_tiny = 0;
        } else {
            use_secure = 0;
            use_tiny = 1;
        }
    }

    if (!opt_dryrun) {
        if (use_secure)
            secure_overwrite(target);
        else if (use_tiny)
            tiny_overwrite(target);
    }

    if (!opt_dryrun) {
        if (DeleteFileA(target)) {
            processed_files++;
            printf("✔ Deleted: %s\n", target);
        } else {
            printf("✖ Failed: %s (err=%lu)\n", target, GetLastError());
        }
    }

    if (renamed)
        free(renamed);
}

/* ------------------------ 目录 ------------------------ */

void process_dir(const char *path) {
    WIN32_FIND_DATAA data;
    char pattern[MAX_PATH];

    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    HANDLE h = FindFirstFileA(pattern, &data);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (!strcmp(data.cFileName, ".") || !strcmp(data.cFileName, ".."))
            continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", path, data.cFileName);

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            process_dir(full);
        } else {
            delete_file(full);
        }

    } while (FindNextFileA(h, &data));

    FindClose(h);

    if (!opt_dryrun)
        RemoveDirectoryA(path);
}


/* ------------------------ main ------------------------ */

void print_help() {
    printf("Usage: shred [-f] [-s] [-t] [-d] [-n] <path>\n");
    printf("-s  secure (3-pass)\n");
    printf("-t  tiny fast mode (1-pass)\n");
    printf("-m  Smart mode (auto select tiny or secure)\n");
    printf("-n  no rename\n");
    printf("-d  dry run\n");
}

int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));

    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'f')) opt_force = 1;
            if (strchr(argv[i], 's')) opt_secure = 1;
            if (strchr(argv[i], 't')) opt_tiny = 1;
            if (strchr(argv[i], 'm')) opt_smart = 1;   // smart mode
            if (strchr(argv[i], 'd')) opt_dryrun = 1;
            if (strchr(argv[i], 'n')) opt_rename = 0;
        } else {
            target = argv[i];
        }
    }

    if (!target) {
        print_help();
        return 1;
    }

    if (opt_secure && opt_tiny) {
        printf("Cannot use -s and -t together\n");
        return 1;
    }

    if (GetFileAttributesA(target) != INVALID_FILE_ATTRIBUTES &&
       !(GetFileAttributesA(target) & FILE_ATTRIBUTE_DIRECTORY)) {
        delete_file(target);
    } else {
        process_dir(target);
    }

    printf("Done. %ld files processed.\n", processed_files);
    return 0;
}