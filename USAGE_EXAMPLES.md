# ShredX 安全删除工具 - 使用示例

## 新增功能：随机重命名

现在 ShredX 在删除文件前会先将文件重命名为随机名称（时间戳+随机数），增强安全性。

## 使用方法

### 基本用法
```cmd
# 标准安全删除（包含随机重命名）
shredx file.txt

# 强制删除，无需确认
shredx -f file.txt

# 安全模式：随机重命名 + 3次随机覆写 + 删除
shredx -s file.txt

# 预览模式：查看将要处理的文件
shredx -d folder/

# 禁用重命名功能
shredx -n file.txt

# 组合选项：强制 + 安全模式
shredx -fs file.txt
```

### 安全等级说明

| 选项组合 | 重命名 | 覆写 | 描述 |
|---------|-------|------|------|
| `shredx file.txt` | ✅ 随机名 | ❌ | 标准删除（重命名+删除） |
| `shredx -s file.txt` | ✅ 随机名 | ✅ 3次 | 高安全删除 |
| `shredx -n file.txt` | ❌ | ❌ | 简单删除 |
| `shredx -ns file.txt` | ❌ | ✅ 3次 | 仅覆写删除 |

### 安全删除流程

1. **随机重命名**: 文件被重命名为随机名称（格式：tmp_时间戳_随机数_随机数_随机数），混淆原始文件名
2. **安全覆写** (可选): 用随机数据多次覆写文件内容
3. **最终删除**: 删除重命名后的文件

这种方法可以防止：
- 文件名恢复
- 文件内容恢复（使用 -s 选项时）
- 目录结构分析

## 编译说明

### C 版本
无需额外库依赖：

```cmd
# 自动编译
build-c.bat

# 手动编译
tcc -Wall -O2 -o shredx.exe shredx.c
```

### Go 版本
需要 Go 1.16+ 和相关依赖：

```cmd
# 自动编译
build-go.bat

# 手动编译
go build -o shredx.exe main.go

# 下载依赖
go mod tidy
```

### Go 版本使用示例

```cmd
# 标准安全删除（包含随机重命名）
shredx file.txt

# 强制删除，无需确认
shredx -f file.txt

# 安全模式：随机重命名 + 3次随机覆写 + 删除
shredx -s file.txt

# 预览模式：查看将要处理的文件
shredx -d folder/

# 禁用重命名功能
shredx -n file.txt

# 设置并发线程数
shredx -t 8 folder/

# 组合选项：强制 + 安全模式 + 8线程
shredx -fst 8 folder/
```