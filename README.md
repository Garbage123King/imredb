# imredb - redb 数据库文件分析器

一个功能强大的 redb 数据库文件解析器和可视化分析工具。

## 功能特性

- **文件解析**：完整解析 redb 数据库文件格式
- **多种视图**：
  - 总览视图：显示文件基本信息和统计
  - 树视图：以树形结构展示 B 树
  - 页面列表：列出所有页面及其详细信息
  - 键值列表：显示所有键值对，支持搜索和过滤
  - 原始数据：以十六进制格式查看页面内容
- **跨平台支持**：提供 GUI 和 CLI 两种工具

## redb 文件格式说明

redb 是一个纯 Rust 编写的嵌入式键值数据库，使用写时复制 (COW) B 树存储数据。

### 文件结构

```
┌────────────────────────────────────────┐
│  超级头 (Super Header) - 512 字节       │
│  - 魔数 (Magic Number)                  │
│  - 上帝字节 (God Byte)                  │
│  - 页大小、区域信息等                    │
├────────────────────────────────────────┤
│  事务槽 1 (Commit Slot) - 128 字节      │
├────────────────────────────────────────┤
│  事务槽 2 (Commit Slot) - 128 字节      │
├────────────────────────────────────────┤
│  数据页面 0                             │
├────────────────────────────────────────┤
│  数据页面 1                             │
├────────────────────────────────────────┤
│  ...                                    │
└────────────────────────────────────────┘
```

### 页面类型

| 类型值 | 名称 | 说明 |
|--------|------|------|
| 1 | 叶子页 (Leaf) | 存储键值对数据 |
| 2 | 分支页 (Branch) | 存储子页指针和索引键 |

### 文件格式版本

- **v1**: 初始版本
- **v2**: 增加 B 树长度字段
- **v3**: 优化区域管理

## 构建说明

### 前置要求

- CMake 3.10+
- 支持 C++17 的编译器
- GLFW3 (用于 GUI 版本)
- OpenGL 3.0+ (用于 GUI 版本)
- **ImGui 1.92.5+** (用于 GUI 版本)

### 下载 ImGui

GUI 版本需要 ImGui 库。请手动下载并解压到项目根目录：

```bash
# 下载 ImGui
curl -L https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5.tar.gz -o imgui-1.92.5.tar.gz
tar -xzf imgui-1.92.5.tar.gz
# 确保目录名为 imgui-1.92.5
```

### Windows (使用 vcpkg)

```powershell
# 安装依赖
vcpkg install glfw3:x64-windows

# 创建构建目录
mkdir build
cd build

# 配置并构建
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux/macOS

```bash
# 安装依赖 (Ubuntu/Debian)
sudo apt-get install libglfw3-dev libopengl-dev

# 创建构建目录
mkdir build
cd build

# 配置并构建
cmake ..
make
```

## 使用方法

### GUI 版本

运行 `imgui_demo` 或 `redb_analyzer.exe`：

```bash
# Windows
./build/imgui_demo.exe
./build/redb_analyzer.exe my_db.redb

# Linux/macOS
./build/imgui_demo
./build/redb_analyzer my_db.redb
```

GUI 界面提供以下功能：
- 菜单栏：文件、视图、帮助
- 工具栏：显示当前打开文件的信息
- 五种视图模式切换
- 键盘快捷键支持

### 命令行版本

运行 `redb_cli`：

```bash
# 基本用法 - 显示文件总览
./build/redb_cli my_db.redb

# 显示页面列表
./build/redb_cli -p my_db.redb

# 显示所有键值对
./build/redb_cli -k my_db.redb

# 搜索键
./build/redb_cli -s "my_key" my_db.redb

# 显示指定页面的原始数据
./build/redb_cli -r 0 my_db.redb

# 查看帮助
./build/redb_cli --help
```

## 命令行选项

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息 |
| `-o, --overview` | 显示文件总览 |
| `-p, --pages` | 显示页面列表 |
| `-k, --key-values` | 显示所有键值对 |
| `-r, --raw <num>` | 显示指定页面的原始数据 |
| `-s, --search <key>` | 搜索包含指定键的记录 |

## 项目结构

```
imredb/
├── CMakeLists.txt          # CMake 配置文件
├── README.md               # 项目说明
├── LICENSE                 # 许可证
├── main.cpp                # GUI 主程序入口
├── redb_parser.h           # redb 解析器头文件
├── redb_parser.cpp         # redb 解析器实现
├── redb_analyzer.h         # 分析器 UI 头文件
├── redb_analyzer.cpp       # 分析器 UI 实现
├── redb_cli.cpp            # 命令行工具
├── build_gui.bat           # Windows GUI 编译脚本
├── my_redb_demo/           # 示例 redb 数据库
│   └── my_db.redb          # 示例数据文件
└── imgui-1.92.5/           # ImGui 库 (需手动下载)
```

## 示例数据

项目中包含一个示例 redb 数据库文件 `my_redb_demo/my_db.redb`，包含以下测试数据：

- 表名: `my_table`
- 键: `my_key`
- 值: `1234`

使用命令行工具查看：

```bash
./build/redb_cli -k my_redb_demo/my_db.redb
```

## 技术细节

### 超级头结构

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0 | 4 | 魔数 (0x72656462 = "redb") |
| 4 | 1 | 上帝字节 (控制数据库状态) |
| 5 | 1 | 页大小指数 (2^exp = page_size) |
| 6 | 2 | 区域头页数 |
| 8 | 4 | 每区域最大数据页数 |
| 12 | 4 | 完整区域数 |
| 16 | 4 | 尾部区域数据页数 |
| 20 | 492 | 保留字节 |

### 事务槽结构

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0 | 4 | 文件格式版本 |
| 4 | 4 | B 树根页号 |
| 8 | 16 | 根页校验和 (XXH3-128) |
| 24 | 8 | B 树元素数量 (v2+) |
| 32 | 8 | 事务 ID |
| 40 | 16 | 槽校验和 |
| 56 | 72 | 保留字节 |

### 页面结构

**叶子页 (Leaf Page)**
```
+----------------------------------+
| 类型(1) | 保留(1) | 条目数(2)     |
+----------------------------------+
| [键结束偏移]... [值结束偏移]...   |
+----------------------------------+
| 键数据 | 值数据                    |
+----------------------------------+
```

**分支页 (Branch Page)**
```
+----------------------------------+
| 类型(2) | 保留(2) | 键数(2)       |
+----------------------------------+
| [子页校验和][子页页号]...         |
+----------------------------------+
| [键结束偏移]...                   |
+----------------------------------+
| 键数据                            |
+----------------------------------+
```

## 许可证

本项目遵循原 redb 数据库的许可证。

## 参考资料

- [redb 官方网站](https://www.redb.org/)
- [redb GitHub 仓库](https://github.com/cberner/redb)
- [redb 设计文档](https://github.com/cberner/redb/blob/master/docs/design.md)
