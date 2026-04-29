# shredx - Advanced Secure File Deletion Tool

`shredx` is a fast, secure, and concurrent file deletion CLI tool written in Go.
It supports secure overwrite, tiny overwrite, parallel deletion, colored logs, and progress bars.

---

## Features

| Feature | Description |
|----------|-------------|
| Concurrent deletion | Multi-threaded file removal |
| Secure mode | Overwrite file contents before deleting (3 passes) |
| Tiny mode | Partially overwrite file contents before deleting |
| Dry-run preview | Show files that would be deleted |
| Colored output | Success, warning, and error messages in color |
| Progress bar | Real-time deletion progress |
| Configurable | Thread count, force mode, secure erase |
| Cross-platform | Works on Windows, macOS, and Linux |

---

## Installation

```bash
git clone https://github.com/bruceasu/shredx.git
cd shredx
go mod tidy
go build -o shredx main.go
```

## Usage

`shredx [flags] <path>`

### Flags

> -f, --force    Skip confirmation prompt
> -s, --secure   Secure mode (overwrite file before deletion)
> -t, --tiny     Fast mode (partial single-pass overwrite before deletion)
> -d, --dry-run  Preview mode (show files but don't delete)
> -n, --no-rename Skip random rename before deletion
> -j, --threads  Number of concurrent workers (default: CPU count)
> -h, --help     Show help message

## Examples

```bash
# Normal deletion (with confirmation)
shredx /tmp/mydir

# Force delete (no prompt)
shredx -f /tmp/mydir

# Secure delete with random overwrite
shredx -f -s /tmp/secret.txt

# Fast delete with tiny overwrite
shredx -f --tiny /tmp/cache.bin

# Preview what will be deleted
shredx -d /tmp/test

# Use 8 threads for faster deletion
shredx -f -s --threads 8 /tmp/largefolder
```

## How It Works

Scans the target path recursively to collect files.

Optionally renames files to random names before deletion.

Optionally overwrites files with random data using secure mode (3 passes) or tiny mode (partial single-pass).

Deletes files concurrently using worker goroutines.

Shows progress bar and color-coded messages.

Safely removes the directory after all files are gone.
