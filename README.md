# 🧹 shredx — Advanced Secure File Deletion Tool

`shredx` is a fast, secure, and concurrent file deletion CLI tool written in Go.  
It supports **secure overwrite**, **parallel deletion**, **colored logs**, and **progress bars** —  
perfect for securely erasing sensitive data on Linux, macOS, or Windows.

---

## ✨ Features

| Feature | Description |
|----------|-------------|
| 🔥 Concurrent deletion | Multi-threaded file removal |
| 🔒 Secure mode | Overwrite file contents before deleting |
| 🧩 Dry-run preview | Show files that would be deleted |
| 🌈 Colored output | Success, warning, and error messages in color |
| 📊 Progress bar | Real-time deletion progress |
| ⚙️ Configurable | Thread count, force mode, secure erase |
| 🧱 Cross-platform | Works on Windows, macOS, and Linux |

---

## 📦 Installation

```bash
# Clone the repository
git clone https://github.com/bruceasu/shredx.git
cd shredx

# Initialize Go module (如果是新项目)
go mod tidy


# Build binary
go build -o shredx main.go
```
## 🧰 Usage
`shredx [flags] <path>`

### Flags
> Flag	Description
> -f, --force	Skip confirmation prompt
> -s, --secure	Secure mode (overwrite file before deletion)
> -d, --dry-run	Preview mode (show files but don’t delete)
> -t, --threads	Number of concurrent workers (default: CPU count)
> -h, --help	Show help message

# 🧪 Examples
```bash
# Normal deletion (with confirmation)
shredx /tmp/mydir

# Force delete (no prompt)
shredx -f /tmp/mydir

# Secure delete with random overwrite
shredx -f -s /tmp/secret.txt

# Preview what will be deleted
shredx -d /tmp/test

# Use 8 threads for faster deletion
shredx -f -s -t 8 /tmp/largefolder

```
# 🖥️ Output Example
> 🧹 Found 152 files.
> Deleting ██████████████▉ 150/152
> ✅ Deletion complete!


or when something fails:

> Failed: /tmp/data/file3.log (permission denied)

# 🧩 Project Structure

> shredx/
> ├── cmd/
> │   └── root.go       # CLI logic using cobra
> ├── main.go           # Entry point
> └── go.mod            # Go module definition

# 🧱 How It Works

Scans the target path recursively to collect files.

Optionally overwrites files with random data (3 passes).

Deletes files concurrently using worker goroutines.

Shows progress bar and color-coded messages.

Safely removes the directory after all files are gone.


# 🧩 Dependencies
```bash

go get github.com/spf13/cobra@latest
go get github.com/fatih/color@latest
go get github.com/schollz/progressbar/v3@latest

```

or simply:

`go mod tidy`

# 🧱 Future Enhancements

 Add logging to file (e.g. --log /path/to/log.txt)

 Support pattern-based deletion (e.g. --match "*.tmp")

 Add .shredxignore for exclusions

 Implement multiple secure erase algorithms (DoD, Gutmann)

 Add system protection for / or C:\Windows

# 🪪 License

MIT License © 2025 [Your Name]
