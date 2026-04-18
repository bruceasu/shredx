import os
import argparse
import secrets
import sys
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed

lock = threading.Lock()
progress = {"done": 0, "total": 0}


# ---------------------------
# 磁盘类型检测
# ---------------------------
def is_ssd(path):
    try:
        if sys.platform.startswith("linux"):
            stat = os.stat(path)
            dev = os.major(stat.st_dev)
            sys_path = f"/sys/dev/block/{dev}:0/queue/rotational"
            if os.path.exists(sys_path):
                with open(sys_path) as f:
                    return f.read().strip() == "0"
        elif sys.platform.startswith("win"):
            # 简化版：默认当作SSD
            return True
    except:
        pass
    return True  # 默认SSD（更通用）


# ---------------------------
# 工具函数
# ---------------------------
def confirm(prompt):
    return input(f"{prompt} [y/N]: ").strip().lower() == "y"


def random_name():
    return secrets.token_hex(8)


# ---------------------------
# 擦除逻辑
# ---------------------------
def secure_wipe(file_path, interactive=False):
    if interactive:
        if not confirm(f"Delete '{file_path}'?"):
            update_progress()
            return

    try:
        size = os.path.getsize(file_path)

        # 重命名
        dir_name = os.path.dirname(file_path)
        new_path = os.path.join(dir_name, random_name())
        os.rename(file_path, new_path)

        with open(new_path, "r+b") as f:
            if size > 0:
                points = {0, size // 2, max(size - 1024, 0)}

                if size > 4096:
                    points.add(secrets.randbelow(size - 1024))

                for pos in points:
                    f.seek(pos)
                    chunk = min(1024, size - pos)
                    f.write(os.urandom(chunk))

            f.flush()
            os.fsync(f.fileno())

            f.truncate(0)
            f.flush()
            os.fsync(f.fileno())

        os.remove(new_path)

    except Exception as e:
        print(f"[ERR] {file_path}: {e}")

    update_progress()


# ---------------------------
# 进度显示
# ---------------------------
def update_progress():
    with lock:
        progress["done"] += 1
        done = progress["done"]
        total = progress["total"]
        percent = (done / total) * 100 if total else 100
        print(f"\r[{done}/{total}] {percent:.1f}% ", end="", flush=True)


# ---------------------------
# 收集文件
# ---------------------------
def collect_files(paths, recursive):
    files = []
    for path in paths:
        if os.path.isfile(path):
            files.append(path)
        elif os.path.isdir(path):
            if recursive:
                for root, _, fs in os.walk(path):
                    for f in fs:
                        files.append(os.path.join(root, f))
            else:
                for f in os.listdir(path):
                    full = os.path.join(path, f)
                    if os.path.isfile(full):
                        files.append(full)
    return files


# ---------------------------
# 主函数
# ---------------------------
def main():
    parser = argparse.ArgumentParser(description="Fast secure delete tool")
    parser.add_argument("paths", nargs="+")
    parser.add_argument("-r", "--recursive", action="store_true")
    parser.add_argument("-i", "--interactive", action="store_true")
    parser.add_argument("-j", "--jobs", type=int, help="Number of threads")

    args = parser.parse_args()

    files = collect_files(args.paths, args.recursive)
    progress["total"] = len(files)

    if not files:
        print("No files found.")
        return

    # 判断磁盘类型（用第一个文件）
    ssd = is_ssd(files[0])

    if args.jobs:
        workers = args.jobs
    else:
        workers = os.cpu_count() if ssd else 2

    print(f"Detected: {'SSD' if ssd else 'HDD'} | Threads: {workers}")

    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [
            executor.submit(secure_wipe, f, args.interactive)
            for f in files
        ]
        for _ in as_completed(futures):
            pass

    print("\nDone.")


if __name__ == "__main__":
    main()
```
