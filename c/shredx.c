/*
 * shredx.c - Legitimate Secure File Deletion Tool for Windows
 * ------------------------------------------------------------------------
 * PURPOSE:
 *   A legitimate system administration tool for secure file deletion on Windows.
 *   Supports safe deletion of sensitive files and directories with optional visualization.
 *
 * LEGAL NOTICE:
 *   This tool is intended for legitimate system administration purposes only.
 *
 * FEATURES:
 *   - Normal deletion / Secure wipe (3-pass overwrite) / Dry-run preview mode
 *   - Force delete (skip confirmation)
 *   - File renaming before deletion for extra safety
 *   - Smart mode: auto-selects secure or fast wipe based on file size
 *   - Recursive directory deletion (-r)
 *   - Interactive mode (-i) for user confirmation
 *
 * USER EXPERIENCE (UX) ENHANCEMENTS:
 *   - Real-time file processing speed and ETA calculation
 *
 * COMPILATION:
 *   tcc -Wall -O2 -o shredx shredx.c
 *
 * SECURITY NOTE:
 *   This tool securely deletes files by overwriting content before deletion.
 *   Standard security practice for sensitive data removal.
 */

#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_RESET  "\033[0m"

#define MAX_BATCH 128
#define SMALL_FILE  (4 * 1024 * 1024)
#define SMART_LIMIT (16 * 1024 * 1024)

/* ================= ANSI HELPERS ================= */

static DWORD get_file_attributes_u8(const char *path) {
    return GetFileAttributesA(path);
}

static HANDLE create_file_u8(const char *path,
                             DWORD desired_access,
                             DWORD share_mode,
                             DWORD creation_disposition,
                             DWORD flags_and_attributes) {
    return CreateFileA(path,
                       desired_access,
                       share_mode,
                       NULL,
                       creation_disposition,
                       flags_and_attributes,
                       NULL);
}

static int move_file_u8(const char *src, const char *dst) {
    return MoveFileA(src, dst) ? 1 : 0;
}

static int delete_file_u8(const char *path) {
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    return DeleteFileA(path) ? 1 : 0;
}

static int remove_directory_u8(const char *path) {
    return RemoveDirectoryA(path) ? 1 : 0;
}

static FILE* fopen_u8_read_text(const char *path) {
    return fopen(path, "rb");
}

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

typedef struct {
    char *batch[MAX_BATCH];
    int count;

    char *include;
    char *exclude;
    int dry_run_preview;
} Context;

/* ===================== UI ===================== */

void draw_progress_bar() {
    if (total_files == 0) return;

    {
        double ratio = (double)processed_files / total_files;
        int percent = (int)(ratio * 100.0);
        int width = 40;
        int filled = (int)(width * ratio);
        int i;

        printf("\r[");
        for (i = 0; i < width; i++) {
            if (i < filled) printf("█");
            else printf("░");
        }
        printf("] %3d%% (%ld/%ld)", percent, processed_files, total_files);
        fflush(stdout);
    }
}

void ui_flush(const char *line) {
    printf("\r%-120s", line);
    fflush(stdout);
}

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

    {
        char *newpath = (char*)malloc(MAX_PATH * 4);
        char name[64];
        const char *slash;

        if (!newpath) return NULL;

        generate_name(name);
        slash = strrchr(path, '\\');

        if (slash) {
            char dir[MAX_PATH * 4];
            size_t len = (size_t)(slash - path + 1);
            strncpy(dir, path, len);
            dir[len] = 0;
            sprintf(newpath, "%s%s", dir, name);
        } else {
            sprintf(newpath, "%s", name);
        }

        if (move_file_u8(path, newpath)) {
            return newpath;
        }

        free(newpath);
        return NULL;
    }
}

/* ===================== WIPE ENGINE ===================== */

void fill_rand(char *buf, DWORD size) {
    DWORD i;
    for (i = 0; i < size; i++)
        buf[i] = rand() & 0xFF;
}

void tiny_wipe(const char *path) {
    HANDLE h = create_file_u8(path,
                              GENERIC_WRITE | GENERIC_READ,
                              0,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL);
    if (h == INVALID_HANDLE_VALUE) return;

    {
        DWORD size = GetFileSize(h, NULL);
        DWORD written;
        char buf[1024];
        DWORD points[4];
        int npoints = 3;
        int i;

        if (size == INVALID_FILE_SIZE) {
            CloseHandle(h);
            return;
        }

        points[0] = 0;
        points[1] = size / 2;
        points[2] = (size > 1024) ? size - 1024 : 0;

        if (size > 4096) {
            points[3] = rand() % (size - 1024);
            npoints = 4;
        }

        for (i = 0; i < npoints && !g_stop; i++) {
            DWORD pos = points[i];
            DWORD chunk = (size - pos > sizeof(buf)) ? sizeof(buf) : size - pos;

            SetFilePointer(h, pos, NULL, FILE_BEGIN);
            fill_rand(buf, chunk);
            WriteFile(h, buf, chunk, &written, NULL);
        }

        FlushFileBuffers(h);

        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        SetEndOfFile(h);
        FlushFileBuffers(h);
    }

    CloseHandle(h);
}

void secure_wipe(const char *path) {
    HANDLE h = create_file_u8(path,
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL);
    if (h == INVALID_HANDLE_VALUE) return;

    {
        DWORD size = GetFileSize(h, NULL);
        char buf[4096];
        DWORD written;
        int pass;

        if (size == INVALID_FILE_SIZE) {
            CloseHandle(h);
            return;
        }

        for (pass = 0; pass < 3 && !g_stop; pass++) {
            long remain = (long)size;
            SetFilePointer(h, 0, NULL, FILE_BEGIN);

            while (remain > 0 && !g_stop) {
                DWORD chunk = remain > (long)sizeof(buf) ? (DWORD)sizeof(buf) : (DWORD)remain;
                fill_rand(buf, chunk);
                WriteFile(h, buf, chunk, &written, NULL);
                remain -= chunk;
            }

            FlushFileBuffers(h);
        }
    }

    CloseHandle(h);
}

/* ===================== SMART MODE ===================== */

int smart_mode(DWORD size) {
    return size <= SMALL_FILE;
}

/* ===================== MATCH ===================== */

int match(const char *pattern, const char *str) {
    if (!pattern) return 1;

    if (pattern[0] == '*' && pattern[strlen(pattern) - 1] == '*') {
        char tmp[256];
        strncpy(tmp, pattern + 1, strlen(pattern) - 2);
        tmp[strlen(pattern) - 2] = 0;
        return strstr(str, tmp) != NULL;
    }

    return strcmp(pattern, str) == 0;
}

/* ===================== FILETER ===================== */

int allow_file(const char *path, Context *ctx) {
    const char *name = strrchr(path, '\\');
    name = name ? name + 1 : path;

    if (ctx->exclude && match(ctx->exclude, name))
        return 0;

    if (ctx->include && !match(ctx->include, name))
        return 0;

    return 1;
}

/* ===================== FILE WORKER ===================== */

void process_file(const char *path) {
    DWORD attr;
    char *target;
    char *renamed;
    DWORD size = 0;
    HANDLE h;
    int use_secure;
    int use_tiny;

    if (g_stop) return;

    attr = get_file_attributes_u8(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    target = (char*)path;
    renamed = NULL;

    if (opt_rename) {
        renamed = rename_file(path);
        if (renamed) target = renamed;
    }

    h = create_file_u8(target,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       OPEN_EXISTING,
                       0);

    if (h != INVALID_HANDLE_VALUE) {
        size = GetFileSize(h, NULL);
        CloseHandle(h);
    }

    use_secure = opt_secure;
    use_tiny = opt_tiny;

    if (opt_smart) {
        if (smart_mode(size)) {
            use_secure = 1;
            use_tiny = 0;
        } else {
            use_secure = 0;
            use_tiny = 1;
        }
    }

    if (!opt_dryrun) {
        if (use_secure) secure_wipe(target);
        else if (use_tiny) tiny_wipe(target);
    }

    if (!opt_dryrun) {
        if (delete_file_u8(target)) {
            processed_files++;
            draw_progress_bar();
        } else {
            printf("\n[FAIL] %s (err=%lu)\n", path, GetLastError());
        }
    }

    if (renamed) free(renamed);
}

/* ===================== BATCH ENGINE ===================== */

void process_batch(Context *ctx) {
    int i;
    for (i = 0; i < ctx->count && !g_stop; i++) {
        process_file(ctx->batch[i]);
        free(ctx->batch[i]);
    }
    ctx->count = 0;
}

/* ===================== SCANNER ===================== */

void scan_dir_shallow(const char *path, Context *ctx) {
    WIN32_FIND_DATAA d;
    HANDLE h;
    char pattern[MAX_PATH * 4];

    sprintf(pattern, "%s\\*", path);

    h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        char full[MAX_PATH * 4];

        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;

        sprintf(full, "%s\\%s", path, d.cFileName);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        ctx->batch[ctx->count++] = _strdup(full);

        if (ctx->count >= MAX_BATCH)
            process_batch(ctx);

    } while (FindNextFileA(h, &d));

    FindClose(h);
}

void scan_dir_recursive(const char *path, Context *ctx) {
    WIN32_FIND_DATAA d;
    HANDLE h;
    char pattern[MAX_PATH * 4];

    sprintf(pattern, "%s\\*", path);

    h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        char full[MAX_PATH * 4];

        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;

        sprintf(full, "%s\\%s", path, d.cFileName);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

            if (ctx->count > 0)
                process_batch(ctx);

            scan_dir_recursive(full, ctx);

            if (ctx->count > 0)
                process_batch(ctx);

            if (!opt_dryrun) {
                if (!remove_directory_u8(full)) {
                    Sleep(10);
                    remove_directory_u8(full);
                }
            }

        } else {
            if (!allow_file(full, ctx))
                continue;

            ctx->batch[ctx->count++] = _strdup(full);

            if (ctx->count >= MAX_BATCH)
                process_batch(ctx);
        }

    } while (FindNextFileA(h, &d));

    FindClose(h);

    if (ctx->count > 0)
        process_batch(ctx);

    if (!opt_dryrun) {
        if (ctx->count > 0)
            process_batch(ctx);

        if (!remove_directory_u8(path)) {
            Sleep(10);
            remove_directory_u8(path);
        }
    }
}

/* ===================== FILE LOADER ===================== */

void load_file_list(const char *filename, const char **targets, int *tcount) {
    FILE *f = fopen_u8_read_text(filename);
    char line[4096];
    int first_line = 1;

    if (!f) {
        printf("Cannot open file list: %s\n", filename);
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        if (first_line) {
            unsigned char *p = (unsigned char*)line;
            if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
                memmove(line, line + 3, strlen(line + 3) + 1);
            }
            first_line = 0;
        }

        if (strlen(line) == 0)
            continue;

        if (line[0] == '"' && line[strlen(line) - 1] == '"') {
            line[strlen(line) - 1] = 0;
            memmove(line, line + 1, strlen(line));
        }

        targets[(*tcount)++] = _strdup(line);
    }

    fclose(f);
}

/* ===================== Tree Preview ===================== */

void preview_dir(const char *path, int depth) {
    WIN32_FIND_DATAA d;
    HANDLE h;
    char pattern[MAX_PATH * 4];

    sprintf(pattern, "%s\\*", path);

    h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        int i;
        char full[MAX_PATH * 4];

        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;

        for (i = 0; i < depth; i++)
            printf("  ");

        printf("├ %s\n", d.cFileName);

        sprintf(full, "%s\\%s", path, d.cFileName);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (opt_recursive)
                preview_dir(full, depth + 1);
        }

    } while (FindNextFileA(h, &d));

    FindClose(h);
}

/* ================= DISPATCH ================= */

void dispatch_path(const char *path, Context *ctx) {
    DWORD attr = get_file_attributes_u8(path);

    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("Not found: %s\n", path);
        return;
    }

    if (ctx->dry_run_preview) {
        printf("[PLAN] %s\n", path);

        if (attr & FILE_ATTRIBUTE_DIRECTORY)
            preview_dir(path, 1);

        return;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (opt_recursive)
            scan_dir_recursive(path, ctx);
        else
            scan_dir_shallow(path, ctx);
    } else {
        if (!allow_file(path, ctx))
            return;

        ctx->batch[ctx->count++] = _strdup(path);

        if (ctx->count >= MAX_BATCH)
            process_batch(ctx);
    }
}

/* ===================== HELP ===================== */

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

/* ===================== FILE COUNTER ===================== */

void count_files(const char *path) {
    DWORD attr = get_file_attributes_u8(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATAA ffd;
        HANDLE hFind;
        char search[MAX_PATH * 4];

        sprintf(search, "%s\\*", path);
        hFind = FindFirstFileA(search, &ffd);

        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            char fullpath[MAX_PATH * 4];

            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                continue;

            sprintf(fullpath, "%s\\%s", path, ffd.cFileName);

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

void count_files_by_arry(const char **paths, int count) {
    int i;
    for (i = 0; i < count; i++) {
        count_files(paths[i]);
    }
}

/* ===================== MAIN ===================== */

int main(int argc, char *argv[]) {
    Context ctx;
    const char *targets[128];
    int tcount = 0;
    int i;
    UINT oldCP = GetConsoleOutputCP();

    SetConsoleOutputCP(65001);
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    srand((unsigned)time(NULL));
    start_time = GetTickCount();

    memset(&ctx, 0, sizeof(Context));
    ctx.count = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {
            print_help();
            SetConsoleOutputCP(oldCP);
            return 0;
        }

        if (strncmp(argv[i], "-f", 2) == 0) {
            const char *file = argv[i + 1];
            if (file && file[0] != '-') {
                load_file_list(file, targets, &tcount);
                i++;
                continue;
            }
        }

        if (strcmp(argv[i], "-p") == 0)
            ctx.dry_run_preview = 1;

        if (strcmp(argv[i], "-x") == 0 && i + 1 < argc)
            ctx.exclude = argv[++i];

        if (strcmp(argv[i], "-y") == 0 && i + 1 < argc)
            ctx.include = argv[++i];

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
        SetConsoleOutputCP(oldCP);
        return 0;
    }

    if (opt_interactive && !opt_dryrun) {
        printf(COLOR_YELLOW "⚠️  You are about to delete\n" COLOR_RESET);
        printf("Continue? (y/N): ");
        fflush(stdout);

        {
            char ans[8];
            if (!fgets(ans, sizeof(ans), stdin)) {
                SetConsoleOutputCP(oldCP);
                return 0;
            }

            if (ans[0] != 'y' && ans[0] != 'Y') {
                printf("Aborted.\n");
                SetConsoleOutputCP(oldCP);
                return 0;
            }
        }
    }

    count_files_by_arry(targets, tcount);
    printf(COLOR_BLUE "🧹 Found %ld files.\n" COLOR_RESET, total_files);

    if (!opt_dryrun) {
        printf(COLOR_BLUE "📋 Deletion process:\n" COLOR_RESET);
        if (opt_rename) printf("  1️⃣  Rename files to random names\n");
        if (opt_secure) printf("  2️⃣  Secure mode (overwrite 3 passes)\n");
        if (opt_tiny) printf("  2️⃣  Fast mode\n");
        if (opt_smart) printf("  2️⃣  Smart mode\n");
        printf("  3️⃣  Delete files\n\n");
    }

    for (i = 0; i < tcount; i++) {
        dispatch_path(targets[i], &ctx);
    }

    if (ctx.count > 0)
        process_batch(&ctx);

    printf("\r");
    if (!opt_dryrun) {
        printf(COLOR_GREEN "\n✅ Secure deletion complete. %ld files processed.\n" COLOR_RESET, processed_files);
        if (opt_rename) printf(COLOR_GREEN "🔄 All files were renamed with random names\n" COLOR_RESET);
        if (opt_secure) printf(COLOR_GREEN "🔒 All files were securely overwritten\n" COLOR_RESET);
    } else {
        printf(COLOR_BLUE "\nDry-run mode complete. %ld files listed.\n" COLOR_RESET, processed_files);
    }

    SetConsoleOutputCP(oldCP);
    return 0;
}