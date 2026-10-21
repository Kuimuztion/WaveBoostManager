/**
 * @file create_window.h
 * @brief 代肝任务面板 - 新建板块（代肝委托）对话框接口
 * @details 负责弹出模态交互窗口，采集代肝委托的标题、委托人信息与初始详细需求，
 *          并在创建持久化成功后触发回调以通知主界面即时更新数据。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef CREATE_WINDOW_H
#define CREATE_WINDOW_H

#include <gtk/gtk.h>

/**
 * @brief 任务创建成功后的异步通知回调函数原型
 * @param user_data 注册回调时传入的上下文数据指针（通常为主窗口 MainWindow* 实例）
 */
typedef void (*TaskCreatedCallback)(gpointer user_data);

/**
 * @brief 构建并显示“创建新板块”模态对话框
 * @details 弹出独立的模态子窗口，阻断对父窗口的操作，直到用户完成创建或点击取消。
 * @param parent 父级窗口句柄，用于建立 GTK 窗口依附层级关系（transient-for）
 * @param on_created 任务持久化成功后的回调通知指针；若无需通知可传 NULL
 * @param user_data 传递给回调函数的上下文自定义载荷指针
 */
void create_window_show(GtkWindow *parent, TaskCreatedCallback on_created, gpointer user_data);

#endif /* CREATE_WINDOW_H */
