/**
 * @file ui_theme.h
 * @brief 代肝任务面板 - UI 视觉主题管理系统接口
 * @details 负责 GTK4 样式层（CSS）的加载。
 *          全局默认仅采用暗色主题（Dark Theme），不支持切换明亮主题。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <gtk/gtk.h>

/**
 * @brief 初始化应用程序视觉主题系统
 * @details 创建全局 GtkCssProvider，并将其注入当前默认 Display 的样式上下文中。
 *          默认且仅启用电竞暗色主题（Dark Theme）。
 */
void ui_theme_init(void);

/**
 * @brief 查询当前是否处于暗黑主题模式
 * @return gboolean 始终返回 TRUE
 */
gboolean ui_theme_is_dark(void);

#endif /* UI_THEME_H */
