package cmd

import (
	"crypto/rand"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync"
	"time"

	"github.com/fatih/color"
	"github.com/schollz/progressbar/v3"
	"github.com/spf13/cobra"
)

// Config 用于保存命令行参数配置
type Config struct {
	Path     string // 删除目标路径
	Force    bool   // 是否跳过确认
	Secure   bool   // 是否安全擦除
	DryRun   bool   // 是否仅预览
	NoRename bool   // 是否禁用重命名
	Threads  int    // 并发线程数
}

// Execute 启动命令行程序
func Execute() {
	if err := rootCmd.Execute(); err != nil {
		os.Exit(1)
	}
}

var cfg = &Config{}

// rootCmd 是主命令定义
var rootCmd = &cobra.Command{
	Use:   "shredx [flags] <path>",
	Short: "shredx - Advanced Secure File Deletion Tool",
	Long: `shredx is a fast and secure file deletion utility.
It supports concurrent deletion, secure overwrite, dry-run preview, and colorized output.`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		cfg.Path = args[0]
		return run(cfg)
	},
}

func init() {
	rootCmd.Flags().BoolVarP(&cfg.Force, "force", "f", false, "Force delete without confirmation")
	rootCmd.Flags().BoolVarP(&cfg.Secure, "secure", "s", false, "Secure delete by overwriting file before removal")
	rootCmd.Flags().BoolVarP(&cfg.DryRun, "dry-run", "d", false, "Preview files to be deleted without deleting them")
	rootCmd.Flags().BoolVarP(&cfg.NoRename, "no-rename", "n", false, "Skip rename step (use original filename)")
	rootCmd.Flags().IntVarP(&cfg.Threads, "threads", "t", runtime.NumCPU(), "Number of concurrent deletion workers")
}

// run 为主逻辑执行函数
func run(cfg *Config) error {
	if !cfg.Force && !cfg.DryRun {
		color.Yellow("⚠️  You are about to delete: %s", cfg.Path)
		if !cfg.NoRename {
			color.Blue("🔄 Files will be renamed to random names before deletion")
		}
		if cfg.Secure {
			color.Blue("🔒 Files will be securely overwritten before deletion")
		}
		fmt.Print("Proceed? (y/N): ")
		var ans string
		fmt.Scanln(&ans)
		if ans != "y" && ans != "Y" {
			color.Cyan("Aborted.")
			return nil
		}
	}

	// 收集所有文件路径
	files := []string{}
	filepath.Walk(cfg.Path, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			color.Red("Failed to access %s: %v", path, err)
			return nil
		}
		if !info.IsDir() {
			files = append(files, path)
		}
		return nil
	})

	if len(files) == 0 {
		color.Blue("No files found.")
		return nil
	}

	color.Green("🧹 Found %d files.", len(files))
	if cfg.DryRun {
		for _, f := range files {
			if !cfg.NoRename {
				fmt.Printf("→ %s (would rename to random name then delete)\n", f)
			} else {
				fmt.Println("→", f)
			}
		}
		color.Cyan("Dry-run mode finished. No files deleted.")
		return nil
	}

	// 显示操作步骤
	color.Blue("📋 Deletion process:")
	if !cfg.NoRename {
		color.Blue("  1️⃣  Rename files to random names")
	}
	if cfg.Secure {
		color.Blue("  2️⃣  Secure overwrite (3 passes)")
	}
	color.Blue("  3️⃣  Delete files")
	fmt.Println()

	// 初始化进度条
	bar := progressbar.NewOptions(len(files),
		progressbar.OptionSetDescription("Deleting"),
		progressbar.OptionShowCount(),
		progressbar.OptionSetWidth(20),
		progressbar.OptionClearOnFinish(),
	)

	// 并发删除任务
	jobs := make(chan string, 100)
	wg := sync.WaitGroup{}

	for i := 0; i < cfg.Threads; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for originalPath := range jobs {
				targetPath := originalPath

				// 第一步：重命名文件（如果启用）
				if !cfg.NoRename {
					if renamedPath, err := renameFileSecurely(originalPath); err != nil {
						color.Red("⚠️  Rename failed: %s (%v), proceeding with original name", originalPath, err)
					} else {
						targetPath = renamedPath
					}
				}

				// 第二步：安全覆写（如果启用）
				if cfg.Secure {
					if err := overwriteFile(targetPath); err != nil {
						color.Red("⚠️  Overwrite failed: %s (%v)", targetPath, err)
					}
				}

				// 第三步：删除文件
				if err := os.Remove(targetPath); err != nil {
					color.Red("Failed: %s (%v)", targetPath, err)
				}
				bar.Add(1)
			}
		}()
	}

	for _, f := range files {
		jobs <- f
	}
	close(jobs)
	wg.Wait()

	os.RemoveAll(cfg.Path)
	color.Green("✅ Secure deletion complete!")
	if !cfg.NoRename {
		color.Green("🔄 All files were renamed with random names")
	}
	if cfg.Secure {
		color.Green("🔒 All files were securely overwritten")
	}
	return nil
}

// generateUniqueName 生成唯一文件名（时间戳+随机数）
func generateUniqueName() string {
	timestamp := time.Now().Unix()

	// 生成3个随机数
	randomBytes := make([]byte, 12) // 4 bytes * 3 = 12 bytes
	rand.Read(randomBytes)

	rand1 := uint32(randomBytes[0])<<24 | uint32(randomBytes[1])<<16 | uint32(randomBytes[2])<<8 | uint32(randomBytes[3])
	rand2 := uint32(randomBytes[4])<<24 | uint32(randomBytes[5])<<16 | uint32(randomBytes[6])<<8 | uint32(randomBytes[7])
	rand3 := uint32(randomBytes[8])<<24 | uint32(randomBytes[9])<<16 | uint32(randomBytes[10])<<8 | uint32(randomBytes[11])

	return fmt.Sprintf("tmp_%08x_%08x_%08x_%08x", timestamp, rand1, rand2, rand3)
}

// renameFileSecurely 安全重命名文件到同一目录下的随机名称
func renameFileSecurely(originalPath string) (string, error) {
	dir := filepath.Dir(originalPath)
	uniqueName := generateUniqueName()
	newPath := filepath.Join(dir, uniqueName)

	err := os.Rename(originalPath, newPath)
	if err != nil {
		return "", err
	}

	// 显示重命名信息
	color.Yellow("🔄 Renamed: %s -> %s", filepath.Base(originalPath), uniqueName)
	return newPath, nil
}

// overwriteFile 进行安全擦除操作（随机覆盖三次）
func overwriteFile(path string) error {
	info, err := os.Stat(path)
	if err != nil {
		return err
	}
	size := info.Size()
	if size == 0 {
		return nil
	}

	f, err := os.OpenFile(path, os.O_WRONLY, 0)
	if err != nil {
		return err
	}
	defer f.Close()

	buf := make([]byte, 4096)
	for pass := 0; pass < 3; pass++ {
		var written int64
		for written < size {
			rand.Read(buf) // 随机数据写入
			n, err := f.WriteAt(buf, written)
			if err != nil {
				return err
			}
			written += int64(n)
		}
		f.Sync()
	}
	return nil
}
