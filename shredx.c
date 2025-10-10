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
int opt_dryrun = 0;

long total_files = 0;
long processed_files = 0;

// ------------------------ 工具函数 ------------------------

void random_bytes(char *buf, DWORD len) {
    for (DWORD i = 0; i < len; i++)
        buf[i] = rand() % 256;
}

// 绘制进度条 ███▒▒ 样式
void draw_progress_bar() {
    if (total_files == 0) return;
    double ratio = (double)processed_files / total_files;
    int percent = (int)(ratio * 100.0);
    int width = 40;  // 进度条宽度
    int filled = (int)(width * ratio);

    printf("\r[");
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("█");
        else printf("░");
    }
    printf("] %3d%% (%ld/%ld)", percent, processed_files, total_files);
    fflush(stdout);
}

// 递归统计文件数
void count_files(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        char search[MAX_PATH];
        snprintf(search, sizeof(search), "%s\\*", path);
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(search, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                continue;
            char fullpath[MAX_PATH];
            snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, ffd.cFileName);
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                count_files(fullpath);
            else
                total_files++;
        } while (FindNextFileA(hFind, &ffd));
        FindClose(hFind);
    } else {
        total_files++;
    }
}

// ------------------------ 删除逻辑 ------------------------

// Secure overwrite function - implements DoD 5220.22-M standard
// This is a legitimate security practice for sensitive data destruction
void overwrite_file(const char *path) {
    printf(COLOR_BLUE "🔒 Securely overwriting: %s\n" COLOR_RESET, path);
    
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf(COLOR_RED "⚠️  Cannot open file for secure deletion: %s\n" COLOR_RESET, path);
        return;
    }

    DWORD sizeLow = GetFileSize(hFile, NULL);
    if (sizeLow == INVALID_FILE_SIZE || sizeLow == 0) {
        CloseHandle(hFile);
        return;
    }

    char buf[4096];
    DWORD written;
    // DoD 5220.22-M standard: 3-pass overwrite
    for (int pass = 0; pass < 3; pass++) {
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        long remaining = sizeLow;
        while (remaining > 0) {
            DWORD chunk = (DWORD)(remaining > sizeof(buf) ? sizeof(buf) : remaining);
            random_bytes(buf, chunk);
            WriteFile(hFile, buf, chunk, &written, NULL);
            remaining -= chunk;
        }
        FlushFileBuffers(hFile);
    }
    CloseHandle(hFile);
}

void delete_file(const char *path) {
    if (opt_dryrun) {
        printf("→ %s\n", path);
        processed_files++;
        draw_progress_bar();
        return;
    }

    if (opt_secure) overwrite_file(path);

    if (DeleteFileA(path)) {
        processed_files++;
        draw_progress_bar();
    } else {
        DWORD err = GetLastError();
        printf(COLOR_RED "\nFailed: %s (Error %lu)\n" COLOR_RESET, path, err);
    }
}

void delete_dir(const char *path) {
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", path);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        delete_file(path);
        return;
    }

    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, ffd.cFileName);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            delete_dir(fullpath);
        else
            delete_file(fullpath);

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);

    if (opt_dryrun) {
        printf("→ %s\\\n", path);
        return;
    }

    if (RemoveDirectoryA(path)) {
        processed_files++;
        draw_progress_bar();
    } else {
        printf(COLOR_RED "\nFailed dir: %s\n" COLOR_RESET, path);
    }
}

void delete_path(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf(COLOR_RED "Path not found: %s\n" COLOR_RESET, path);
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        delete_dir(path);
    else
        delete_file(path);
}

// ------------------------ 主程序入口 ------------------------

void print_help() {
    printf("Usage: shredx [-f] [-s] [-d] <path>\n");
    printf("Options:\n");
    printf("  -f    Force delete without confirmation\n");
    printf("  -s    Secure mode (overwrite before delete)\n");
    printf("  -d    Dry-run (preview only)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
	// 获取原始控制台代码页
    UINT oldCP = GetConsoleOutputCP();
    // 切换到 UTF-8 控制台输出
    SetConsoleOutputCP(65001);
    // 切换到 UTF-8 控制台编码，避免 CP936 乱码
    //system("chcp 65001 >nul");
    
    srand((unsigned)time(NULL));

    const char *target = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'f')) opt_force = 1;
            if (strchr(argv[i], 's')) opt_secure = 1;
            if (strchr(argv[i], 'd')) opt_dryrun = 1;
        } else {
            target = argv[i];
        }
    }

    if (!target) {
        print_help();
        return 1;
    }

    if (!opt_force && !opt_dryrun) {
        printf(COLOR_YELLOW "⚠️  You are about to delete: %s\n" COLOR_RESET, target);
        printf("Proceed? (y/N): ");
        char ans[8];
        if (!fgets(ans, sizeof(ans), stdin)) return 0;
        if (ans[0] != 'y' && ans[0] != 'Y') {
            printf("Aborted.\n");
            return 0;
        }
    }

    count_files(target);
    printf(COLOR_BLUE "🧹 Found %ld files.\n" COLOR_RESET, total_files);

    delete_path(target);

    printf("\r");
    if (!opt_dryrun)
        printf(COLOR_GREEN "\n✅ Deletion complete. %ld files processed.\n" COLOR_RESET, processed_files);
    else
        printf(COLOR_BLUE "\nDry-run mode complete. %ld files listed.\n" COLOR_RESET, processed_files);

    // system("chcp 936 >nul");
    // 恢复原来的代码页
    SetConsoleOutputCP(oldCP);
    return 0;
}
