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

type Config struct {
	Path     string
	Force    bool
	Secure   bool
	Tiny     bool
	DryRun   bool
	NoRename bool
	Threads  int
}

func Execute() {
	if err := rootCmd.Execute(); err != nil {
		os.Exit(1)
	}
}

var cfg = &Config{}

var rootCmd = &cobra.Command{
	Use:   "shredx [flags] <path>",
	Short: "shredx - Advanced Secure File Deletion Tool",
	Long: `shredx is a fast and secure file deletion utility.
It supports concurrent deletion, secure overwrite, tiny overwrite, dry-run preview, and colorized output.`,
	Args: cobra.ExactArgs(1),
	PreRunE: func(cmd *cobra.Command, args []string) error {
		if cfg.Secure && cfg.Tiny {
			return fmt.Errorf("--secure and --tiny cannot be used together")
		}
		return nil
	},
	RunE: func(cmd *cobra.Command, args []string) error {
		cfg.Path = args[0]
		return run(cfg)
	},
}

func init() {
	rootCmd.Flags().BoolVarP(&cfg.Force, "force", "f", false, "Force delete without confirmation")
	rootCmd.Flags().BoolVarP(&cfg.Secure, "secure", "s", false, "Secure delete by overwriting file before removal")
	rootCmd.Flags().BoolVarP(&cfg.Tiny, "tiny", "t", false, "Fast delete by partially overwriting file before removal")
	rootCmd.Flags().BoolVarP(&cfg.DryRun, "dry-run", "d", false, "Preview files to be deleted without deleting them")
	rootCmd.Flags().BoolVarP(&cfg.NoRename, "no-rename", "n", false, "Skip rename step (use original filename)")
	rootCmd.Flags().IntVarP(&cfg.Threads, "threads", "j", runtime.NumCPU(), "Number of concurrent deletion workers")
}

func run(cfg *Config) error {
	if !cfg.Force && !cfg.DryRun {
		color.Yellow("You are about to delete: %s", cfg.Path)
		if !cfg.NoRename {
			color.Blue("Files will be renamed to random names before deletion")
		}
		if cfg.Secure {
			color.Blue("Files will be securely overwritten before deletion")
		}
		if cfg.Tiny {
			color.Blue("Files will be partially overwritten in tiny mode before deletion")
		}
		fmt.Print("Proceed? (y/N): ")

		var ans string
		fmt.Scanln(&ans)
		if ans != "y" && ans != "Y" {
			color.Cyan("Aborted.")
			return nil
		}
	}

	files := []string{}
	err := filepath.Walk(cfg.Path, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			color.Red("Failed to access %s: %v", path, err)
			return nil
		}
		if !info.IsDir() {
			files = append(files, path)
		}
		return nil
	})
	if err != nil {
		return err
	}

	if len(files) == 0 {
		color.Blue("No files found.")
		return nil
	}

	color.Green("Found %d files.", len(files))
	if cfg.DryRun {
		for _, f := range files {
			switch {
			case !cfg.NoRename && cfg.Secure:
				fmt.Printf("-> %s (would rename, secure overwrite, then delete)\n", f)
			case !cfg.NoRename && cfg.Tiny:
				fmt.Printf("-> %s (would rename, tiny overwrite, then delete)\n", f)
			case !cfg.NoRename:
				fmt.Printf("-> %s (would rename to random name then delete)\n", f)
			case cfg.Secure:
				fmt.Printf("-> %s (would secure overwrite then delete)\n", f)
			case cfg.Tiny:
				fmt.Printf("-> %s (would tiny overwrite then delete)\n", f)
			default:
				fmt.Printf("-> %s\n", f)
			}
		}
		color.Cyan("Dry-run mode finished. No files deleted.")
		return nil
	}

	color.Blue("Deletion process:")
	if !cfg.NoRename {
		color.Blue("  1. Rename files to random names")
	}
	if cfg.Secure {
		color.Blue("  2. Secure overwrite (3 passes)")
	}
	if cfg.Tiny {
		color.Blue("  2. Tiny overwrite (partial single-pass)")
	}
	color.Blue("  3. Delete files")
	fmt.Println()

	bar := progressbar.NewOptions(len(files),
		progressbar.OptionSetDescription("Deleting"),
		progressbar.OptionShowCount(),
		progressbar.OptionSetWidth(20),
		progressbar.OptionClearOnFinish(),
	)

	jobs := make(chan string, 100)
	var wg sync.WaitGroup

	for i := 0; i < cfg.Threads; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for originalPath := range jobs {
				targetPath := originalPath

				if !cfg.NoRename {
					renamedPath, err := renameFileSecurely(originalPath)
					if err != nil {
						color.Red("Rename failed: %s (%v), proceeding with original name", originalPath, err)
					} else {
						targetPath = renamedPath
					}
				}

				if cfg.Secure {
					if err := overwriteFile(targetPath); err != nil {
						color.Red("Overwrite failed: %s (%v)", targetPath, err)
					}
				}

				if cfg.Tiny {
					if err := tinyOverwriteFile(targetPath); err != nil {
						color.Red("Tiny overwrite failed: %s (%v)", targetPath, err)
					}
				}

				if err := os.Remove(targetPath); err != nil {
					color.Red("Failed: %s (%v)", targetPath, err)
				}
				_ = bar.Add(1)
			}
		}()
	}

	for _, f := range files {
		jobs <- f
	}
	close(jobs)
	wg.Wait()

	if err := os.RemoveAll(cfg.Path); err != nil && !os.IsNotExist(err) {
		color.Red("Failed to remove path %s (%v)", cfg.Path, err)
	}

	color.Green("Secure deletion complete!")
	if !cfg.NoRename {
		color.Green("All files were renamed with random names")
	}
	if cfg.Secure {
		color.Green("All files were securely overwritten")
	}
	if cfg.Tiny {
		color.Green("All files were partially overwritten in tiny mode")
	}
	return nil
}

func generateUniqueName() string {
	timestamp := time.Now().Unix()
	randomBytes := make([]byte, 12)
	_, _ = rand.Read(randomBytes)

	rand1 := uint32(randomBytes[0])<<24 | uint32(randomBytes[1])<<16 | uint32(randomBytes[2])<<8 | uint32(randomBytes[3])
	rand2 := uint32(randomBytes[4])<<24 | uint32(randomBytes[5])<<16 | uint32(randomBytes[6])<<8 | uint32(randomBytes[7])
	rand3 := uint32(randomBytes[8])<<24 | uint32(randomBytes[9])<<16 | uint32(randomBytes[10])<<8 | uint32(randomBytes[11])

	return fmt.Sprintf("tmp_%08x_%08x_%08x_%08x", timestamp, rand1, rand2, rand3)
}

func renameFileSecurely(originalPath string) (string, error) {
	dir := filepath.Dir(originalPath)
	uniqueName := generateUniqueName()
	newPath := filepath.Join(dir, uniqueName)

	if err := os.Rename(originalPath, newPath); err != nil {
		return "", err
	}

	color.Yellow("Renamed: %s -> %s", filepath.Base(originalPath), uniqueName)
	return newPath, nil
}

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
			chunk := int64(len(buf))
			if remaining := size - written; remaining < chunk {
				chunk = remaining
			}
			if _, err := rand.Read(buf[:chunk]); err != nil {
				return err
			}
			n, err := f.WriteAt(buf[:chunk], written)
			if err != nil {
				return err
			}
			written += int64(n)
		}
		if err := f.Sync(); err != nil {
			return err
		}
	}
	return nil
}

func tinyOverwriteFile(path string) error {
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

	const chunkSize int64 = 1024
	points := []int64{0, size / 2}
	if size > chunkSize {
		points = append(points, size-chunkSize)
	} else {
		points = append(points, 0)
	}
	if size > 4096 {
		randomPoint, err := randomOffset(size - chunkSize)
		if err != nil {
			return err
		}
		points = append(points, randomPoint)
	}

	buf := make([]byte, chunkSize)
	for _, pos := range points {
		remaining := size - pos
		if remaining <= 0 {
			continue
		}

		chunk := chunkSize
		if remaining < chunk {
			chunk = remaining
		}
		if _, err := rand.Read(buf[:chunk]); err != nil {
			return err
		}
		if _, err := f.WriteAt(buf[:chunk], pos); err != nil {
			return err
		}
	}

	return f.Sync()
}

func randomOffset(max int64) (int64, error) {
	if max <= 0 {
		return 0, nil
	}

	var b [8]byte
	if _, err := rand.Read(b[:]); err != nil {
		return 0, err
	}

	var value uint64
	for _, v := range b {
		value = (value << 8) | uint64(v)
	}

	return int64(value % uint64(max)), nil
}
