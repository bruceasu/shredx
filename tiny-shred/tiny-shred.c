/* Tiny-shred */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#define CHUNK_SIZE (1024 * 1024) // 1MB

int interactive = 0;
int recursive = 0;

/* ---------------------------
   随机数据填充
--------------------------- */
void fill_random(unsigned char *buf, size_t size) {
#ifdef _WIN32
    for (size_t i = 0; i < size; i++) {
        buf[i] = rand() % 256; // 简化版（Windows可换更安全API）
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("urandom");
        exit(1);
    }
    read(fd, buf, size);
    close(fd);
#endif
}

/* ---------------------------
   覆盖并删除文件
--------------------------- */
void wipe_file(const char *path) {
    if (interactive) {
        printf("Delete '%s'? [y/N]: ", path);
        int c = getchar();
        if (c != 'y' && c != 'Y') {
            printf("[SKIP] %s\n", path);
            while (c != '\n' && c != EOF) c = getchar();
            return;
        }
        while (c != '\n' && c != EOF) c = getchar();
    }

    FILE *f = fopen(path, "rb+");
    if (!f) {
        perror(path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    unsigned char *buffer = malloc(CHUNK_SIZE);
    if (!buffer) {
        perror("malloc");
        fclose(f);
        return;
    }

    long remaining = size;

    while (remaining > 0) {
        size_t chunk = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
        fill_random(buffer, chunk);

        if (fwrite(buffer, 1, chunk, f) != chunk) {
            perror("write");
            break;
        }

        remaining -= chunk;
    }

    fflush(f);


    #ifdef _WIN32
        fflush(f);
        _commit(_fileno(f));   // TCC/MinGW 更兼容
    #else
        fsync(fileno(f));
    #endif

    fclose(f);
    free(buffer);

    if (remove(path) == 0) {
        printf("[OK] %s\n", path);
    } else {
        perror("remove");
    }
}

/* ---------------------------
   处理路径
--------------------------- */
void process_path(const char *path);

void process_dir(const char *path) {
    WIN32_FIND_DATAA data;
    char pattern[4096];

    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    HANDLE h = FindFirstFileA(pattern, &data);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0)
            continue;

        char full[4096];
        snprintf(full, sizeof(full), "%s\\%s", path, data.cFileName);

        DWORD attr = GetFileAttributesA(full);

        if (attr == INVALID_FILE_ATTRIBUTES)
            continue;

        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive)
                process_dir(full);
        } else {
            wipe_file(full);
        }

    } while (FindNextFileA(h, &data));

    FindClose(h);
}

void process_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISREG(st.st_mode)) {
        wipe_file(path);
    } else if (S_ISDIR(st.st_mode)) {
        process_dir(path);
    }
}

/* ---------------------------
   main
--------------------------- */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: wipe [-r] [-i] files...\n");
        return 1;
    }

    int start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            recursive = 1;
            start++;
        } else if (strcmp(argv[i], "-i") == 0) {
            interactive = 1;
            start++;
        }
    }

    for (int i = start; i < argc; i++) {
        process_path(argv[i]);
    }

    return 0;
}

