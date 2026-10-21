/**
 * @file ui_theme.c
 * @brief 代肝任务面板 - UI 视觉主题管理系统实现
 * @details 实现了 GTK4 应用层 CSS 样式的注入与管理。
 *          全局锁定为纯黑沉浸主题（OLED Pure Black Theme），
 *          彻底剔除任何浅色与纯白背景，保障全交互状态高对比度。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "ui_theme.h"
#include <stdio.h>

/**
 * @brief 全局单例 CSS 样式提供器句柄
 * @details 注入 GdkDisplay 并赋予 USER 级别最高优先级以完全覆盖系统主题。
 */
static GtkCssProvider *s_provider = NULL;

/**
 * @brief 纯黑主题（Pure Black Theme）完整样式表
 * @details 设计规范：
 *          - 背景色：#000000（纯黑 OLED 底色）
 *          - 顶部面板：#0A0A0A，底部分界线 #1E1E1E
 *          - 卡片底色：#111111，边框 #222222，悬浮 #191919
 *          - 主行动色：#5B6EF5，激活 #3849BE，文字恒为白色
 *          - 次要按钮：#161616，悬浮 #242424，激活 #303030
 *          - 输入控件：#121212，边框 #2A2A2A，聚焦高亮 #5B6EF5
 */
static const char *DARK_CSS =
/* 1. 顶层窗口与对话框纯黑覆盖，杜绝系统主题纯白/浅灰侵入 */
"window, window.background, .background, dialog, .main-window {"
"    background-color: #000000;"
"    background: #000000;"
"    color: #F0F0F5;"
"    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'PingFang SC', 'Microsoft YaHei', sans-serif;"
"}"
/* 2. 所有滚动容器与视口背景纯黑化 */
"scrolledwindow, scrolledwindow viewport, viewport {"
"    background-color: #000000;"
"    background: #000000;"
"    border: none;"
"    box-shadow: none;"
"}"
/* 3. 顶部导航栏面板 */
".header-panel {"
"    background-color: #0A0A0A;"
"    padding: 16px 24px;"
"    border-bottom: 1px solid #1E1E1E;"
"}"
/* 4. 任务卡片基础样式及平滑悬浮过渡 */
".task-card {"
"    background-color: #111111;"
"    border-radius: 10px;"
"    padding: 14px 18px;"
"    margin-bottom: 12px;"
"    border: 1px solid #222222;"
"    transition: background-color 150ms ease-in-out;"
"}"
".task-card:hover {"
"    background-color: #191919;"
"}"
/* 5. 任务序号徽章（胶囊状） */
".rank-badge {"
"    background-color: #1E1E1E;"
"    color: #7B8EFF;"
"    font-weight: bold;"
"    font-size: 11px;"
"    border-radius: 18px;"
"    min-width: 38px;"
"    min-height: 38px;"
"}"
/* 6. 导航栏新建主按钮：点击激活时文字恒白，背景高亮，决不变黑隐形 */
".btn-create-nav {"
"    background-color: #5B6EF5;"
"    color: #FFFFFF;"
"    border-radius: 7px;"
"    padding: 8px 18px;"
"    font-size: 13px;"
"    font-weight: bold;"
"    border: 1px solid #5B6EF5;"
"    box-shadow: none;"
"}"
".btn-create-nav:hover {"
"    background-color: #4A5DE0;"
"    color: #FFFFFF;"
"    border-color: #4A5DE0;"
"}"
".btn-create-nav:active, .btn-create-nav:checked {"
"    background-color: #3849BE;"
"    color: #FFFFFF;"
"    border-color: #8AA0FF;"
"    box-shadow: 0 0 0 2px rgba(91, 110, 245, 0.4);"
"}"
".btn-create-nav:focus {"
"    color: #FFFFFF;"
"    outline: 2px solid #8AA0FF;"
"}"
/* 7. 导航次要功能按钮（历史记录等） */
".btn-nav {"
"    background-color: #161616;"
"    color: #E0E0E0;"
"    border-radius: 7px;"
"    padding: 8px 16px;"
"    font-size: 13px;"
"    font-weight: 500;"
"    border: 1px solid #2E2E2E;"
"    box-shadow: none;"
"}"
".btn-nav:hover {"
"    background-color: #242424;"
"    color: #FFFFFF;"
"    border-color: #3E3E3E;"
"}"
".btn-nav:active, .btn-nav:checked {"
"    background-color: #303030;"
"    color: #FFFFFF;"
"    border-color: #5B6EF5;"
"}"
".btn-nav:focus {"
"    color: #FFFFFF;"
"    outline: 1px solid #5B6EF5;"
"}"
/* 8. 对话框通用确定主按钮（创建板块、保存修改） */
".btn-primary {"
"    background-color: #5B6EF5;"
"    color: #FFFFFF;"
"    border-radius: 6px;"
"    padding: 9px 20px;"
"    font-size: 13px;"
"    font-weight: bold;"
"    border: 1px solid #5B6EF5;"
"}"
".btn-primary:hover {"
"    background-color: #4A5DE0;"
"    color: #FFFFFF;"
"}"
".btn-primary:active, .btn-primary:checked {"
"    background-color: #3849BE;"
"    color: #FFFFFF;"
"    border-color: #8AA0FF;"
"}"
".btn-primary:focus {"
"    color: #FFFFFF;"
"    outline: 2px solid #8AA0FF;"
"}"
/* 9. 对话框通用次要按钮（取消、关闭） */
".btn-secondary {"
"    background-color: #1A1A1A;"
"    color: #B0B0B0;"
"    border-radius: 6px;"
"    padding: 9px 20px;"
"    font-size: 13px;"
"    font-weight: bold;"
"    border: 1px solid #2C2C2C;"
"}"
".btn-secondary:hover {"
"    background-color: #262626;"
"    color: #FFFFFF;"
"    border-color: #3A3A3A;"
"}"
".btn-secondary:active, .btn-secondary:checked {"
"    background-color: #333333;"
"    color: #FFFFFF;"
"    border-color: #555555;"
"}"
".btn-secondary:focus {"
"    color: #FFFFFF;"
"    outline: 1px solid #555555;"
"}"
/* 10. 任务卡片右侧快捷操作按钮（查看、结算、终止） */
".btn-action {"
"    background-color: #1A1A1A;"
"    border-radius: 6px;"
"    padding: 7px 15px;"
"    font-size: 13px;"
"    font-weight: bold;"
"    border: 1px solid #2A2A2A;"
"    box-shadow: none;"
"}"
".btn-action:hover {"
"    background-color: #242424;"
"}"
".btn-action:active, .btn-action:checked {"
"    background-color: #303030;"
"}"
".btn-view {"
"    color: #FFFFFF;"
"}"
".btn-terminate {"
"    color: #FF5C6A;"
"}"
".btn-settle {"
"    color: #3DDC84;"
"}"
/* 11. 状态胶囊药丸指示器 */
".status-badge {"
"    border-radius: 8px;"
"    padding: 3px 10px;"
"    font-size: 11px;"
"    font-weight: bold;"
"}"
".status-progress {"
"    background-color: #151D3B;"
"    color: #8AA0FF;"
"}"
".status-completed {"
"    background-color: #0E291C;"
"    color: #3DDC84;"
"}"
".status-terminated {"
"    background-color: #2E151B;"
"    color: #FF5C6A;"
"}"
/* 12. 单行文本输入控件（防止系统浅色背景侵犯） */
"entry, entry.custom-entry, entry text, entry.custom-entry text {"
"    background-color: #121212;"
"    color: #F0F0F5;"
"    border: 1px solid #2A2A2A;"
"    border-radius: 6px;"
"    padding: 8px 10px;"
"    font-size: 14px;"
"}"
"entry:focus, entry.custom-entry:focus, entry text:focus {"
"    border-color: #5B6EF5;"
"    background-color: #161616;"
"    color: #FFFFFF;"
"}"
/* 13. 多行富文本区域（详细需求与跟进笔录） */
"textview, textview.custom-textview, textview text,"
"textview.custom-textview text {"
"    background-color: #121212;"
"    background: #121212;"
"    color: #F0F0F5;"
"    border-radius: 6px;"
"    font-size: 14px;"
"}"
"textview.custom-textview {"
"    border: 1px solid #2A2A2A;"
"}"
"textview.custom-textview:focus {"
"    border-color: #5B6EF5;"
"}"
/* 14. 跟进笔录卡片项与列表容器 */
".note-card {"
"    background-color: #121212;"
"    border-radius: 6px;"
"    padding: 10px 12px;"
"    margin-bottom: 8px;"
"    border: 1px solid #1E1E1E;"
"}"
".notes-box {"
"    background-color: #0A0A0A;"
"    border: 1px solid #1A1A1A;"
"    border-radius: 8px;"
"    padding: 8px;"
"}"
/* 15. 排版层级文字配色 */
".text-title {"
"    font-size: 19px;"
"    font-weight: bold;"
"    color: #FFFFFF;"
"}"
".text-subtitle {"
"    font-size: 12px;"
"    color: #9E9E9E;"
"}"
".card-title {"
"    font-size: 16px;"
"    font-weight: bold;"
"    color: #FFFFFF;"
"}"
".card-subinfo {"
"    font-size: 12px;"
"    color: #8E8E8E;"
"}"
".field-label {"
"    font-size: 13px;"
"    font-weight: bold;"
"    color: #B0B0B0;"
"    margin-bottom: 6px;"
"}"
".empty-hint {"
"    font-size: 14px;"
"    color: #777777;"
"    padding: 24px;"
"}";

/**
 * @brief 将纯黑主题样式应用并刷新到 GTK4 显示系统中
 * @details 设置最高 USER 优先级，覆盖系统层（Breeze）的默认浅色背景。
 */
static void apply_theme(void) {
        GtkSettings *settings = gtk_settings_get_default();
        if (settings) {
                g_object_set(settings, "gtk-application-prefer-dark-theme",
                             TRUE, NULL);
        }

        if (!s_provider) {
                s_provider = gtk_css_provider_new();
                GdkDisplay *display = gdk_display_get_default();
                if (display) {
                        gtk_style_context_add_provider_for_display(
                                display,
                                GTK_STYLE_PROVIDER(s_provider),
                                GTK_STYLE_PROVIDER_PRIORITY_USER);
                }
        }

        gtk_css_provider_load_from_string(s_provider, DARK_CSS);
}

/**
 * @brief 初始化界面主题系统
 * @details 应用程序启动时调用，默认且仅加载纯黑主题。
 */
void ui_theme_init(void) {
        apply_theme();
}

/**
 * @brief 查询当前激活的主题是否为暗黑模式
 * @return gboolean 恒定返回 TRUE
 */
gboolean ui_theme_is_dark(void) {
        return TRUE;
}
