/**
 * @file detail_window.h
 * @brief 代肝任务面板 - 板块详情与跟进笔录对话框接口
 * @details 负责展示代肝任务的完整信息、历史起止时间、编辑修改需求说明，
 *          以及追加跟进笔录（如装备强化进度、抽卡出货明细、阶段性交付记录等）。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef DETAIL_WINDOW_H
#define DETAIL_WINDOW_H

#include <gtk/gtk.h>

/**
 * @brief 任务信息或笔录发生更新后的异步回调函数原型
 * @param user_data 注册时传递的上下文指针（通常为主界面 MainWindow* 实例）
 */
typedef void (*TaskUpdatedCallback)(gpointer user_data);

/**
 * @brief 构建并展示任务详情与跟进对话框
 * @details 根据任务当前状态自动判断只读模式：若已结算或已终止，自动锁定表单为只读浏览；
 *          若处于进行中，则开放修改需求与实时追加笔录的完整交互权限。
 * @param parent 父级窗口句柄，确立模态依附关系
 * @param task_id 目标任务的 32 字符十六进制 UUID
 * @param read_only 是否显式强制以只读模式打开（例如在历史记录窗口中查看时传 TRUE）
 * @param on_updated 当需求被修改保存或新增笔录入库后的通知回调
 * @param user_data 传递给回调函数的自定义上下文数据指针
 */
void detail_window_show(GtkWindow *parent, const char *task_id, gboolean read_only, TaskUpdatedCallback on_updated, gpointer user_data);

#endif /* DETAIL_WINDOW_H */
