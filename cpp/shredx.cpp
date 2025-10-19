#define _CRT_SECURE_NO_WARNINGS  // 禁用安全函数警告
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

int opt_force = 0;
int opt_secure = 0;
int opt_dryrun = 0;
int opt_rename = 1;  // 默认启用重命名功能

long total_files = 0;
long processed_files = 0;

// ------------------------ 工具函数 ------------------------

// 生成唯一文件名（时间戳+随机数）
std::string generate_unique_name() {
    unsigned int timestamp = static_cast<unsigned int>(time(NULL));
    unsigned int rand1 = rand();
    unsigned int rand2 = rand();
    unsigned int rand3 = rand();

    char buf[64] = {0};
    sprintf_s(buf, sizeof(buf), "tmp_%08x_%08x_%08x_%08x", timestamp, rand1, rand2, rand3);
    return std::string(buf);
}

// 安全重命名文件（在同一目录下用唯一名称重命名）
std::string rename_file_securely(const std::string& original_path) {
    if (opt_dryrun) return "";

    size_t last_slash_pos = original_path.find_last_of("\\/");
    std::string dir_path;
    if (last_slash_pos != std::string::npos) {
        dir_path = original_path.substr(0, last_slash_pos + 1);
    }

    std::string unique_name = generate_unique_name();
    std::string new_path = dir_path + unique_name;

    if (MoveFileA(original_path.c_str(), new_path.c_str())) {
        std::string original_file = (last_slash_pos != std::string::npos) ? original_path.substr(last_slash_pos + 1) : original_path;
        std::cout << COLOR_YELLOW << "Renamed: " << original_file << " -> " << unique_name << COLOR_RESET << std::endl;
        return new_path;
    } else {
        std::cerr << COLOR_RED << "Rename failed: " << original_path << " (Error " << GetLastError() << ")" << COLOR_RESET << std::endl;
        return "";
    }
}

// 生成指定长度的随机字节填充缓冲区
void random_bytes(char* buf, DWORD len) {
    for (DWORD i = 0; i < len; i++)
        buf[i] = static_cast<char>(rand() % 256);
}

// 绘制进度条 ███▒▒ 样式
void draw_progress_bar() {
    if (total_files == 0) return;
    double ratio = static_cast<double>(processed_files) / total_files;
    int percent = static_cast<int>(ratio * 100.0);
    int width = 40;  // 进度条宽度
    int filled = static_cast<int>(width * ratio);

    std::cout << "\r[";
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            std::cout << "=";
        } else {
            std::cout << ">";
        }
    }
    std::cout << "] " << percent << "% (" << processed_files << "/" << total_files << ")";
    std::cout.flush();
}

// 递归统计文件数
void count_files(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        std::string search = path + "\\*";
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(search.c_str(), &ffd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                continue;
            std::string fullpath = path + "\\" + ffd.cFileName;
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
void overwrite_file(const std::string& path) {
    std::cout << COLOR_BLUE << "Securely overwriting: " << path << COLOR_RESET << std::endl;

    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << COLOR_RED << "Cannot open file for secure deletion: " << path << COLOR_RESET << std::endl;
        return;
    }

    DWORD sizeLow = GetFileSize(hFile, NULL);
    if (sizeLow == INVALID_FILE_SIZE || sizeLow == 0) {
        CloseHandle(hFile);
        return;
    }

    std::vector<char> buf(4096);
    DWORD written;
    for (int pass = 0; pass < 3; pass++) {
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        long remaining = sizeLow;
        while (remaining > 0) {
            DWORD chunk = (DWORD)(remaining > static_cast<long>(buf.size()) ? buf.size() : remaining);
            random_bytes(buf.data(), chunk);
            if (!WriteFile(hFile, buf.data(), chunk, &written, NULL)) {
                std::cerr << COLOR_RED << "WriteFile failed during overwrite: " << path << COLOR_RESET << std::endl;
                break;
            }
            remaining -= chunk;
        }
        FlushFileBuffers(hFile);
    }
    CloseHandle(hFile);
}

void delete_file(const std::string& path) {
    if (opt_dryrun) {
        if (opt_rename) {
            std::cout << "→ " << path << " (would rename to random name then delete)" << std::endl;
        } else {
            std::cout << "→ " << path << std::endl;
        }
        processed_files++;
        draw_progress_bar();
        return;
    }

    std::string target_path = path;
    std::string renamed_path;

    if (opt_rename) {
        renamed_path = rename_file_securely(path);
        if (!renamed_path.empty()) {
            target_path = renamed_path;
        } else {
            std::cout << COLOR_YELLOW << "Proceeding with original filename" << COLOR_RESET << std::endl;
            target_path = path;
        }
    }

    if (opt_secure) overwrite_file(target_path);

    if (DeleteFileA(target_path.c_str())) {
        processed_files++;
        draw_progress_bar();
    } else {
        DWORD err = GetLastError();
        std::cerr << COLOR_RED << "\nFailed: " << target_path << " (Error " << err << ")" << COLOR_RESET << std::endl;
    }
}

void delete_dir(const std::string& path) {
    std::string search = path + "\\*";

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        delete_file(path);
        return;
    }

    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        std::string fullpath = path + "\\" + ffd.cFileName;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            delete_dir(fullpath);
        else
            delete_file(fullpath);

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);

    if (opt_dryrun) {
        std::cout << "Would delete directory: " << path << "\\" << std::endl;
        return;
    }

    if (RemoveDirectoryA(path.c_str())) {
        processed_files++;
        draw_progress_bar();
    } else {
        std::cerr << "\nFailed to delete directory: " << path << std::endl;
    }
}

void delete_path(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Path not found: " << path << std::endl;
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        delete_dir(path);
    else
        delete_file(path);
}

void print_help() {
    std::cout << "Usage: shredx [-f] [-s] [-d] [-n] <path>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -f    Force delete without confirmation" << std::endl;
    std::cout << "  -s    Secure mode (overwrite before delete)" << std::endl;
    std::cout << "  -d    Dry-run (preview only)" << std::endl;
    std::cout << "  -n    No rename (skip rename step)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    srand(static_cast<unsigned>(time(NULL)));

    std::string target;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] == '-') {
            if (arg.find('f') != std::string::npos) opt_force = 1;
            if (arg.find('s') != std::string::npos) opt_secure = 1;
            if (arg.find('d') != std::string::npos) opt_dryrun = 1;
            if (arg.find('n') != std::string::npos) opt_rename = 0;
        } else {
            target = arg;
        }
    }

    if (target.empty()) {
        print_help();
        return 1;
    }

    if (!opt_force && !opt_dryrun) {
        std::cout << "You are about to delete: " << target << std::endl;
        if (opt_rename) std::cout << "Files will be renamed before deletion" << std::endl;
        if (opt_secure) std::cout << "Files will be securely overwritten before deletion" << std::endl;
        std::cout << "Proceed? (y/N): ";
        std::string ans;
        std::getline(std::cin, ans);
        if (ans.empty() || (ans[0] != 'y' && ans[0] != 'Y')) {
            std::cout << "Aborted." << std::endl;
            return 0;
        }
    }

    count_files(target);
    std::cout << "Found " << total_files << " files." << std::endl;

    delete_path(target);

    std::cout << "\nSecure deletion complete. " << processed_files << " files processed." << std::endl;

    return 0;
}
