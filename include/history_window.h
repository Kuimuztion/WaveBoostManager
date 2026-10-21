/**
 * @file history_window.h
 * @brief 代肝任务面板 - 历史记录（已完结/已终止板块）对话框接口
 * @details 负责展示已归档的代肝任务清单，支持以只读模式追溯历史任务的委托详情与笔录。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef HISTORY_WINDOW_H
#define HISTORY_WINDOW_H

#include <gtk/gtk.h>

/**
 * @brief 历史记录窗口关闭时的通知回调函数原型
 * @param user_data 注册时传递的上下文指针（通常为主界面 MainWindow* 实例）
 */
typedef void (*HistoryWindowClosedCallback)(gpointer user_data);

/**
 * @brief 构建并展示历史记录模态对话框
 * @details 从持久化层拉取所有已结算（Completed）与已终止（Terminated）的委托任务，
 *          按完结时间倒序排列展示；提供“查看”按钮可调出只读详情看板。
 * @param parent 父级窗口句柄
 * @param on_closed 窗口关闭时的回调通知（用于通知主界面同步刷新）
 * @param user_data 传递给回调的上下文载荷
 */
void history_window_show(GtkWindow *parent, HistoryWindowClosedCallback on_closed, gpointer user_data);

#endif /* HISTORY_WINDOW_H */
