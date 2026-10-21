/**
 * @file main_window.h
 * @brief 代肝任务面板 - 主工作台窗口接口
 * @details 负责构建应用程序的核心交互界面：顶部导航栏（历史记录入口、主题切换、新建任务）、
 *          中央进行中委托卡片列表（排单序号、任务详情、状态快速变更与结算/终止流转）。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>

/**
 * @brief 主工作台窗口的不透明对象句柄声明
 * @details 隐藏内部 UI 控件树的具体成员细节，维持接口清晰的高内聚架构封装。
 */
typedef struct _MainWindow MainWindow;

/**
 * @brief 创建并初始化主工作台窗口实例
 * @param app 归属的 GtkApplication 上下文指针
 * @return MainWindow* 新建的主窗口句柄指针，内存由其关联的顶层窗口生命周期管理
 */
MainWindow *main_window_new(GtkApplication *app);

/**
 * @brief 刷新主工作台中的进行中任务卡片列表
 * @details 从持久化存储层重新加载所有处于“进行中”状态的委托，
 *          更新排单序号徽章、标题与元信息，动态处理空列表占位提示的显隐。
 * @param win 主窗口句柄指针
 */
void main_window_refresh(MainWindow *win);

/**
 * @brief 获取主工作台底层对应的 GTK 顶层窗口部件指针
 * @param win 主窗口句柄指针
 * @return GtkWindow* 顶层 GtkApplicationWindow 指针，用于建立模态子窗口依附或呈递显示
 */
GtkWindow *main_window_get_window(MainWindow *win);

#endif /* MAIN_WINDOW_H */
