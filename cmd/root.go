package cmd

import (
	"crypto/rand"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync"

	"github.com/fatih/color"
	"github.com/schollz/progressbar/v3"
	"github.com/spf13/cobra"
)

// Config 用于保存命令行参数配置
type Config struct {
	Path    string // 删除目标路径
	Force   bool   // 是否跳过确认
	Secure  bool   // 是否安全擦除
	DryRun  bool   // 是否仅预览
	Threads int    // 并发线程数
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
	rootCmd.Flags().IntVarP(&cfg.Threads, "threads", "t", runtime.NumCPU(), "Number of concurrent deletion workers")
}

// run 为主逻辑执行函数
func run(cfg *Config) error {
	if !cfg.Force && !cfg.DryRun {
		color.Yellow("⚠️  You are about to delete: %s", cfg.Path)
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
			fmt.Println("→", f)
		}
		color.Cyan("Dry-run mode finished. No files deleted.")
		return nil
	}

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
			for f := range jobs {
				if cfg.Secure {
					_ = overwriteFile(f)
				}
				err := os.Remove(f)
				if err != nil {
					color.Red("Failed: %s (%v)", f, err)
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
	color.Green("✅ Deletion complete!")
	return nil
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

