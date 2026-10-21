# WaveBoostManager (代肝任务面板)

一款专为游戏代肝/代练委托管理的现代化 Linux 原生桌面面板，采用纯 C 语言配合 GTK 4 开发。支持按序排单、跟进笔录、电竞深色沉浸主题及历史记录归档。

---

## 编译与安装指南

### 1. 安装编译依赖

本程序依赖 C 编译器、Make、GTK 4 以及 cJSON 库。请根据您的 Linux 发行版执行对应命令安装开发依赖包：

#### Fedora / RHEL (推荐)
```bash
sudo dnf install -y gcc make pkgconf gtk4-devel cjson-devel
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S --needed gcc make pkgconf gtk4 cjson
```

#### Ubuntu / Debian
```bash
sudo apt install -y build-essential pkg-config libgtk-4-dev libcjson-dev
```

---

### 2. 编译与运行

```bash
# 1. 编译构建（构建产物自动输出至 builds/ 目录）
make

# 2. 运行自动化单元测试
make test

# 3. 运行程序
./builds/wave_boost_manager
# 或快捷指令
make run
```

---

### 3. 安装到系统桌面菜单 (可选)

```bash
make install
```

安装后即可在系统应用启动器（KDE Application Launcher / GNOME / Rofi / Wofi）中搜索「代肝任务面板」，或直接在终端任意路径运行 `waveboostmanager` 启动。

如需卸载：
```bash
make uninstall
```

---

### 4. 数据存储位置

在 Linux 环境下，数据遵循 XDG Base Directory 规范存储于：
```text
~/.config/WaveBoostManager/tasks.json
```
每次任务创建、状态流转（结算/终止）或保存修改均会自动进行原子落盘持久化。
