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
#include <signal.h>

#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_RESET  "\033[0m"

#define MAX_BATCH 128
#define SMALL_FILE  (4 * 1024 * 1024)
#define SMART_LIMIT (16 * 1024 * 1024)

typedef struct {
    char *batch[MAX_BATCH];
    int count;
} Context;

/* ================= FLAGS ================= */

int opt_secure = 0;
int opt_tiny = 0;
int opt_smart = 0;
int opt_rename = 1;
int opt_dryrun = 0;
int opt_force = 0;
int opt_recursive = 0;
int opt_interactive = 0;

/* ===================== GLOBAL STATE ===================== */
volatile int g_stop = 0;
long total_files = 0;
long processed_files = 0;
DWORD start_time = 0;


/* ===================== CTRL+C SAFE EXIT ===================== */

BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT) {
        printf("\n[CTRL+C] stopping safely...\n");
        g_stop = 1;
        return TRUE;
    }
    return FALSE;
}

/* ===================== UTIL ===================== */

void generate_name(char *out) {
    sprintf(out, "tmp_%08x_%08x",
            (unsigned)time(NULL), rand());
}

/* ===================== RENAMER ===================== */

char* rename_file(const char *path) {
    if (opt_dryrun) return NULL;

    char *newpath = malloc(MAX_PATH);
    char name[64];
    generate_name(name);

    const char *slash = strrchr(path, '\\');

    if (slash) {
        char dir[MAX_PATH];
        size_t len = slash - path + 1;
        strncpy(dir, path, len);
        dir[len] = 0;
        sprintf(newpath, "%s%s", dir, name);
    } else {
        sprintf(newpath, "%s", name);
    }

    if (MoveFileA(path, newpath)) {
        printf("  rename -> %s\n", name);
        return newpath;
    }

    free(newpath);
    return NULL;
}

/* ===================== WIPE ENGINE ===================== */

void fill_rand(char *buf, DWORD size) {
    for (DWORD i = 0; i < size; i++)
        buf[i] = rand() & 0xFF;
}

void tiny_wipe(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(h);
        return;
    }

    char buf[4096];
    DWORD written;
    long remain = size;

    SetFilePointer(h, 0, NULL, FILE_BEGIN);

    while (remain > 0 && !g_stop) {
        DWORD chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
        fill_rand(buf, chunk);
        WriteFile(h, buf, chunk, &written, NULL);
        remain -= chunk;
    }

    FlushFileBuffers(h);
    CloseHandle(h);
}

void secure_wipe(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(h);
        return;
    }

    char buf[4096];
    DWORD written;

    for (int pass = 0; pass < 3 && !g_stop; pass++) {
        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        long remain = size;

        while (remain > 0 && !g_stop) {
            DWORD chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
            fill_rand(buf, chunk);
            WriteFile(h, buf, chunk, &written, NULL);
            remain -= chunk;
        }

        FlushFileBuffers(h);
    }

    CloseHandle(h);
}

/* ===================== SMART MODE ===================== */

int smart_mode(DWORD size) {
    return size <= SMART_LIMIT;
}

/* ===================== ETA ENGINE ===================== */

void print_eta() {
    DWORD now = GetTickCount();
    float elapsed = (now - start_time) / 1000.0f;

    float rate = processed_files / (elapsed + 0.01f);
    float remain = (total_files - processed_files) / (rate + 0.01f);

    printf("\r[%ld/%ld] ETA: %.1fs   ",
           processed_files, total_files, remain);
}

/* ===================== FILE WORKER ===================== */

void process_file(const char *path) {
    if (g_stop) return;

    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    char *target = (char*)path;

    /* rename */
    char *renamed = NULL;
    if (opt_rename) {
        renamed = rename_file(path);
        if (renamed) target = renamed;
    }

    /* get size */
    DWORD size = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        size = GetFileSize(h, NULL);
        CloseHandle(h);
    }

    int use_secure = opt_secure;
    int use_tiny = opt_tiny;

    if (opt_smart) {
        if (smart_mode(size)) {
            use_secure = 1;
            use_tiny = 0;
        } else {
            use_secure = 0;
            use_tiny = 1;
        }
    }

    /* wipe */
    if (!opt_dryrun) {
        if (use_secure) secure_wipe(target);
        else if (use_tiny) tiny_wipe(target);
        else printf("No mode is chosen.");
    }

    /* delete */
    if (!opt_dryrun) {
        if (DeleteFileA(target)) {
            processed_files++;
            printf("\n[DEL] %s\n", path);
            print_eta();
        } else {
            printf("\n[FAIL] %s\n", path);
        }
    }

    if (renamed) free(renamed);
}

/* ===================== BATCH ENGINE ===================== */

void process_batch(Context *ctx) {
    for (int i = 0; i < ctx->count && !g_stop; i++) {
        process_file(ctx->batch[i]);
        free(ctx->batch[i]);
    }
    ctx->count = 0;
}
/* ===================== SCANNER ===================== */

void scan_dir(const char *path, Context *ctx) {
    WIN32_FIND_DATAA d;
    char pattern[MAX_PATH];

    sprintf(pattern, "%s\\*", path);

    HANDLE h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;

        char full[MAX_PATH];
        sprintf(full, "%s\\%s", path, d.cFileName);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (opt_recursive)
                scan_dir(full, ctx);
        } else {
            ctx->batch[ctx->count++] = _strdup(full);

            if (ctx->count >= MAX_BATCH)
                process_batch(ctx);
        }

    } while (FindNextFileA(h, &d));

    FindClose(h);
}

/* ================= DISPATCH ================= */

void dispatch_path(const char *path, Context *ctx) {
    DWORD attr = GetFileAttributesA(path);

    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("Not found: %s\n", path);
        return;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (opt_recursive)
            scan_dir(path, ctx);
        else
            printf("Skip dir (use -r): %s\n", path);
    } else {
        ctx->batch[ctx->count++] = _strdup(path);

        if (ctx->count >= MAX_BATCH)
            process_batch(ctx);
    }
}

void print_help() {
    printf("\nshredx v3.1 (Windows CLI Secure Deletion Tool)\n\n");

    printf("USAGE:\n");
    printf("  shredx [options] <file|dir> [file|dir...]\n\n");

    printf("OPTIONS:\n");
    printf("  -s   Secure wipe (3-pass overwrite)\n");
    printf("  -t   Tiny wipe (fast single-pass)\n");
    printf("  -m   Smart mode (auto select -s/-t)\n");
    printf("  -r   Recursive directory processing\n");
    printf("  -n   Do not rename (keep original name)\n");
    printf("  -d   Dry run (preview only)\n");
    printf("  -i   Interactive\n");
    printf("  -f   @FILE (Each line a file or directory)\n\n");

    printf("BEHAVIOR:\n");
    printf("  file -> Rename -> wipe -> delete\n");
    printf("  dir  -> (if -r) recursive scan\n\n");

    printf("EXAMPLES:\n");

    printf("  shredx file.txt\n");
    printf("  shredx -t file1.txt file2.txt\n");
    printf("  shredx -s file.txt\n");
    printf("  shredx -m file.txt\n");
    printf("  shredx -r folder\n");
    printf("  shredx -m -r folder file.txt\n\n");

    printf("NOTES:\n");
    printf("  - Smart mode auto switches strategy by file size\n");
    printf("  - Batch engine improves performance on many files\n");
    printf("  - Ctrl+C safely stops processing\n\n");
}

void load_file_list(const char *filename, const char **targets, int *tcount) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Cannot open file list: %s\n", filename);
        return;
    }

    char line[4096];

    while (fgets(line, sizeof(line), f)) {

        // 去掉换行
        line[strcspn(line, "\r\n")] = 0;

        // 跳过空行
        if (strlen(line) == 0)
            continue;

        // 支持带空格路径（去掉外层引号）
        if (line[0] == '"' && line[strlen(line)-1] == '"') {
            line[strlen(line)-1] = 0;
            memmove(line, line + 1, strlen(line));
        }

        targets[(*tcount)++] = _strdup(line);
    }

    fclose(f);
}

/* ===================== MAIN ===================== */

int main(int argc, char *argv[]) {
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    srand((unsigned)time(NULL));
    start_time = GetTickCount();

    Context ctx;
    ctx.count = 0;

    const char *targets[128];
    int tcount = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }

        if (strncmp(argv[i], "-f", 2) == 0) {
            const char *file = argv[i + 1];
            if (file && file[0] != '-') {
                load_file_list(file, targets, &tcount);
                i++; // skip file name
            }
        }

        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'i')) opt_interactive = 1;
            if (strchr(argv[i], 's')) opt_secure = 1;
            if (strchr(argv[i], 't')) opt_tiny = 1;
            if (strchr(argv[i], 'm')) opt_smart = 1;
            if (strchr(argv[i], 'r')) opt_recursive = 1;
            if (strchr(argv[i], 'n')) opt_rename = 0;
            if (strchr(argv[i], 'd')) opt_dryrun = 1;
        } else {
            targets[tcount++] = argv[i];
        }
    }

    if (tcount == 0) {
        print_help();
        return 0;
    }

    if (opt_interactive && !opt_dryrun) {
        printf("\nAbout to process %d targets. Continue? (y/N): ", tcount);

        char ans[8];
        fgets(ans, sizeof(ans), stdin);

        if (ans[0] != 'y' && ans[0] != 'Y') {
            printf("Aborted.\n");
            return 0;
        }
    }

    for (int i = 0; i < tcount; i++) {
        dispatch_path(targets[i], &ctx);
    }

    if (ctx.count > 0)
        process_batch(&ctx);

    printf("\nDone. %ld files processed.\n", processed_files);
    return 0;
}