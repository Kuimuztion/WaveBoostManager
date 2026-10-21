/**
 * @file main.c
 * @brief 代肝任务面板 - 应用程序主程序入口与系统级初始化
 * @details 负责以下关键系统基础设施的初始化与生命周期管控：
 *          1. 关键系统崩溃信号（SIGSEGV、SIGABRT 等）捕获与调用栈转储追踪（Backtrace Crash Log）；
 *          2. Linux 桌面图标主题多层级动态搜索路径装配（适配本地开发、全局安装与便携式运行环境）；
 *          3. GTK4 应用层单实例容器生命周期与主工作台窗口的装载呈现。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <execinfo.h>
#include <unistd.h>

#include "ui_theme.h"
#include "main_window.h"

/**
 * @brief 将程序崩溃异常现场信息与函数调用栈序列化写入磁盘日志
 * @details 在捕获到致命信号时紧急介入，尽可能将故障发生时刻的时间、信号编号、
 *          信号描述文本以及最近 32 层的堆栈调用帧符号提取并持久化落盘。
 *          优先尝试在当前工作目录下创建 `crash_log.txt`，若权限不足则回退至用户配置目录。
 * @param sig 接收到的系统信号编号（例如 SIGSEGV 值为 11）
 */
static void write_crash_log(int sig) {
        /* 
         * 1. 尝试在当前工作目录以追加模式打开 crash_log.txt。
         * 若在只读文件系统或无权限目录下运行，则回退尝试写入 ~/.config/WaveBoostManager/crash_log.txt
         */
        FILE *fp = fopen("crash_log.txt", "a");
        if (!fp) {
                char *path = g_build_filename(g_get_user_config_dir(), "WaveBoostManager", "crash_log.txt", NULL);
                fp = fopen(path, "a");
                g_free(path);
        }

        /* 2. 成功获取可写文件句柄，开始格式化记录崩溃诊断报告 */
        if (fp) {
                /* 获取本地可读时间戳 */
                time_t now = time(NULL);
                char timestr[64];
                strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

                /* 写入崩溃标头与信号说明（strsignal 会将 11 转换为 "Segmentation fault"） */
                fprintf(fp, "[%s] Fatal signal %d (%s) encountered.\n", timestr, sig, strsignal(sig));

                /* 
                 * 3. 利用 glibc execinfo 接口捕获当前线程的调用栈：
                 * backtrace() 获取最近 32 个栈帧的程序计数器地址；
                 * backtrace_symbols() 将地址解析为包含二进制名、函数偏移量和相对地址的可读符号表。
                 */
                void *callstack[32];
                int frames = backtrace(callstack, 32);
                char **strs = backtrace_symbols(callstack, frames);
                if (strs) {
                        for (int i = 0; i < frames; ++i) {
                                fprintf(fp, "  #%d: %s\n", i, strs[i]);
                        }
                        /* backtrace_symbols 内部由 malloc 分配，须显式 free 回收 */
                        free(strs);
                }
                fprintf(fp, "\n");
                fclose(fp);
        }
}

/**
 * @brief 系统致命崩溃信号处理函数
 * @details 负责在遭遇野指针解引用、断言失败等未捕获异常时记录调用栈日志，
 *          随后将信号重置为系统默认行为并重新触发该信号，保证 Linux 核心转储（Core Dump）机制正常生效。
 * @param sig 接收到的系统信号编号
 */
static void crash_handler(int sig) {
        /* 1. 记录崩溃调用栈到文本文件供开发者排查问题 */
        write_crash_log(sig);

        /* 
         * 2. 重置该信号的处理动作为默认动作（SIG_DFL）：
         * 避免再次触发信号时进入无限递归死循环
         */
        signal(sig, SIG_DFL);

        /* 
         * 3. 重新向自身抛出原信号：
         * 让操作系统接管异常终止流程，生成核心转储文件或告知外层 Shell 正确的退出码
         */
        raise(sig);
}

/**
 * @brief 注册关键系统崩溃信号的捕获拦截器
 * @details 覆盖 Linux 下最常见的不可恢复异常信号：
 *          - SIGSEGV：段错误（非法内存访问、空指针解引用）
 *          - SIGABRT：程序异常中止（assert 断言失败或主动调用 abort）
 *          - SIGFPE：算术/浮点异常（除以零或溢出）
 *          - SIGBUS：总线错误（内存地址对齐错误或物理分页失效）
 *          - SIGILL：非法 CPU 指令
 */
static void setup_crash_handlers(void) {
        signal(SIGSEGV, crash_handler);
        signal(SIGABRT, crash_handler);
        signal(SIGFPE,  crash_handler);
        signal(SIGBUS,  crash_handler);
        signal(SIGILL,  crash_handler);
}

/**
 * @brief 配置并装配 GTK4 应用程序图标主题的多层级搜索路径
 * @details 保证应用程序在以下各种复杂的运行环境中均能精准加载应用专属图标（app.png / waveboostmanager.png）：
 *          1. 源码本地调试环境：当前目录 `.` 与 `resources/`；
 *          2. Linux 桌面标准规范安装路径：`~/.local/share/icons` 与 `hicolor/128x128/apps`；
 *          3. 便携式绿色免安装运行：通过 Linux `/proc/self/exe` 符号链接逆向解析可执行文件自身所在目录。
 */
static void setup_icon_theme(void) {
        /* 获取默认图形显示服务句柄 */
        GdkDisplay *display = gdk_display_get_default();
        if (!display) return;

        /* 获取针对当前显示服务的图标主题管理器 */
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        if (!theme) return;

        /* 
         * 层级 1：工作目录本地路径（便于本地源码编译调试与构建测试）
         */
        gtk_icon_theme_add_search_path(theme, ".");
        gtk_icon_theme_add_search_path(theme, "resources");

        /* 
         * 层级 2：标准 XDG 用户数据目录中的用户图标路径（~/.local/share/icons）
         */
        char *user_icons = g_build_filename(g_get_user_data_dir(), "icons", NULL);
        gtk_icon_theme_add_search_path(theme, user_icons);
        g_free(user_icons);

        char *user_hicolor = g_build_filename(g_get_user_data_dir(), "icons", "hicolor", "128x128", "apps", NULL);
        gtk_icon_theme_add_search_path(theme, user_hicolor);
        g_free(user_hicolor);

        /* 
         * 层级 3：基于 /proc/self/exe 解析的动态相对路径：
         * 读取 Linux 虚拟文件系统链接，获取当前运行中的可执行程序绝对路径，
         * 将其所在的同级目录及上层目录注入图标搜索路径，确保便携绿色分发模式下不丢图标。
         */
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
                exe_path[len] = '\0';
                /* 提取可执行程序所在目录 */
                char *dir = g_path_get_dirname(exe_path);
                gtk_icon_theme_add_search_path(theme, dir);

                /* 提取可执行程序父级目录 */
                char *parent = g_path_get_dirname(dir);
                gtk_icon_theme_add_search_path(theme, parent);

                g_free(parent);
                g_free(dir);
        }
}

/**
 * @brief GtkApplication 的 "activate" 信号回调函数
 * @details 当应用正式启动激活（或从外部通过 D-Bus 再次拉起）时执行。
 *          负责配置图标主题、应用初始视觉样式、构建主工作台并呈现顶层窗口。
 * @param app 激活事件发送者 GtkApplication 句柄
 * @param user_data 注册时传递的上下文指针（此处为 NULL）
 */
static void on_activate(GtkApplication *app, gpointer user_data) {
        (void)user_data;

        /* 1. 初始化并拓展图标主题检索路径 */
        setup_icon_theme();

        /* 2. 初始化加载默认暗黑主题 CSS 样式表 */
        ui_theme_init();

        /* 3. 实例化主工作台窗口并获取底层 GtkWindow* 句柄 */
        MainWindow *main_win = main_window_new(app);
        GtkWindow *window = main_window_get_window(main_win);

        /* 4. 设定窗口默认图标为 waveboostmanager */
        gtk_window_set_default_icon_name("waveboostmanager");

        /* 5. 呈递并激活窗口，显示给终端用户 */
        gtk_window_present(window);
}

/**
 * @brief 应用程序主入口函数
 * @param argc 命令行启动参数个数
 * @param argv 命令行启动参数文本数组
 * @return int 应用程序退出状态码（0 表示正常退出）
 */
int main(int argc, char **argv) {
        /* 1. 注册系统底层异常崩溃拦截机制，提供调用栈诊断支持 */
        setup_crash_handlers();

        /* 
         * 2. 创建 GTK4 应用程序实例：
         * 设定应用反向域名标识为 "org.waveboost.manager"，
         * 采用标准默认应用运行标志（单实例、D-Bus 激活支持）
         */
        GtkApplication *app = gtk_application_new("org.waveboost.manager", G_APPLICATION_DEFAULT_FLAGS);

        /* 3. 绑定应用启动激活处理函数 */
        g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

        /* 
         * 4. 移交控制权，进入 GApplication 事件驱动主循环：
         * 监听并分发鼠标键盘事件、GTK 渲染循环与 D-Bus 通信，直至所有窗口关闭
         */
        int status = g_application_run(G_APPLICATION(app), argc, argv);

        /* 5. 退出后解引用应用句柄，清理相关 GLib 上下文 */
        g_object_unref(app);

        return status;
}
