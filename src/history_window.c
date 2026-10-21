/**
 * @file history_window.c
 * @brief 代肝任务面板 - 历史记录对话框实现
 * @details 实现了已结算和已终止任务列表的倒序检索与渲染，
 *          并提供进入只读详情看板的交互逻辑，同时在窗口关闭时触发主界面同步刷新。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "history_window.h"
#include "models.h"
#include "data_service.h"
#include "detail_window.h"

/**
 * @brief 历史记录对话框上下文
 * @details 保存窗口自身部件引用、卡片列表容器、空态提示标签及关闭回调闭包。
 */
typedef struct {
        GtkWidget *window;                     /**< 对话框顶层窗口 */
        GtkWidget *history_list_box;           /**< 历史任务卡片列表容器（GtkBox） */
        GtkWidget *empty_hint_lbl;             /**< 无历史记录时的占位提示标签 */
        HistoryWindowClosedCallback callback;  /**< 窗口关闭时的通知回调 */
        gpointer user_data;                    /**< 传递给回调的上下文载荷 */
} HistoryDialogContext;

/**
 * @brief 查看单条历史任务详情的操作上下文
 * @details 绑定在单条历史卡片的“查看”按钮信号上，保存目标任务的 ID 与对话框上下文指针。
 */
typedef struct {
        HistoryDialogContext *ctx; /**< 归属的历史对话框上下文 */
        char *task_id;             /**< 目标任务的 32 字符 UUID 副本 */
} ViewHistoryContext;

/**
 * @brief 历史卡片“查看”按钮点击事件处理函数
 * @details 打开任务详情对话框，并强制设置 read_only = TRUE，防止历史数据被篡改。
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 关联的 ViewHistoryContext 上下文指针
 */
static void on_view_history_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        ViewHistoryContext *vhc = (ViewHistoryContext *)user_data;

        /* 
         * 打开详情窗口：
         * 第三个参数强制传 TRUE（只读），防止用户在历史记录中修改需求或误加笔录
         */
        detail_window_show(GTK_WINDOW(vhc->ctx->window), vhc->task_id, TRUE, NULL, NULL);
}

/**
 * @brief 释放“查看”按钮私有上下文的回调函数
 * @details 由 g_signal_connect_data 在按钮小部件被销毁或信号解绑时由 GTK 运行时自动调用。
 * @param data ViewHistoryContext 结构体指针
 * @param closure 信号闭包指针（未使用）
 */
static void free_view_ctx(gpointer data, GClosure *closure) {
        (void)closure;
        ViewHistoryContext *vhc = (ViewHistoryContext *)data;

        /* 释放任务 ID 字符串及上下文内存 */
        g_free(vhc->task_id);
        g_free(vhc);
}

/**
 * @brief 重新检索并渲染历史任务卡片列表
 * @details 清空旧卡片，从数据层获取已完结/已终止任务列表并按时间降序渲染。
 * @param ctx 历史对话框上下文
 */
static void render_history(HistoryDialogContext *ctx) {
        /* 
         * 1. 清理已有子项：
         * 保证多次重新渲染时卡片不会发生重复堆叠
         */
        GtkWidget *child = gtk_widget_get_first_child(ctx->history_list_box);
        while (child) {
                GtkWidget *next = gtk_widget_get_next_sibling(child);
                gtk_box_remove(GTK_BOX(ctx->history_list_box), child);
                child = next;
        }

        /* 
         * 2. 加载历史数据：
         * 调用数据服务层获取非进行中任务（已按完结时间倒序排好）
         */
        GPtrArray *history = data_service_get_history();

        /* 
         * 3. 边界情况判断：
         * 若历史记录为空，显式展现空态提示文本，并及时释放数组句柄返回
         */
        if (!history || history->len == 0) {
                gtk_widget_set_visible(ctx->empty_hint_lbl, TRUE);
                if (history) g_ptr_array_unref(history);
                return;
        }

        /* 存在历史任务，隐藏空态占位 */
        gtk_widget_set_visible(ctx->empty_hint_lbl, FALSE);

        /* 
         * 4. 逐条构建历史任务卡片：
         * 布局：水平容器（左侧标题+状态+时间+笔录数量，右侧“查看”按钮）
         */
        for (guint i = 0; i < history->len; i++) {
                BoostTask *task = g_ptr_array_index(history, i);

                /* 卡片外层水平容器 */
                GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
                gtk_widget_add_css_class(card, "task-card");

                /* 左侧综合信息垂直容器（开启水平扩展以将右侧按钮推至末端） */
                GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_set_hexpand(info_box, TRUE);
                gtk_widget_set_valign(info_box, GTK_ALIGN_CENTER);

                /* 信息第一行：任务标题 + 状态胶囊标签 */
                GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                GtkWidget *title_lbl = gtk_label_new(task->title);
                gtk_widget_add_css_class(title_lbl, "card-title");
                gtk_box_append(GTK_BOX(row1), title_lbl);

                /* 根据历史状态着色（绿色已结算 vs 红色已终止） */
                const char *status_str = boost_status_to_string(task->status);
                GtkWidget *status_badge = gtk_label_new(status_str);
                gtk_widget_add_css_class(status_badge, "status-badge");
                if (task->status == BOOST_STATUS_COMPLETED) {
                        gtk_widget_add_css_class(status_badge, "status-completed");
                } else {
                        gtk_widget_add_css_class(status_badge, "status-terminated");
                }
                gtk_box_append(GTK_BOX(row1), status_badge);
                gtk_box_append(GTK_BOX(info_box), row1);

                /* 信息第二行：创建时间、完结时间及笔录统计 */
                char *created_fmt = format_display_time(task->created_at, "%m-%d %H:%M");
                GString *sub_str = g_string_new("");
                g_string_printf(sub_str, "创建于 %s", created_fmt);
                g_free(created_fmt);

                /* 完结时间标注动词（结算于 / 终止于） */
                if (task->finished_at && *task->finished_at) {
                        char *fin_fmt = format_display_time(task->finished_at, "%m-%d %H:%M");
                        const char *verb = (task->status == BOOST_STATUS_COMPLETED) ? "结算" : "终止";
                        g_string_append_printf(sub_str, "    %s于 %s", verb, fin_fmt);
                        g_free(fin_fmt);
                }

                /* 笔录总条数统计 */
                guint note_count = task->notes ? task->notes->len : 0;
                g_string_append_printf(sub_str, "    笔录 %u 条", note_count);

                GtkWidget *sub_lbl = gtk_label_new(sub_str->str);
                g_string_free(sub_str, TRUE);
                gtk_widget_add_css_class(sub_lbl, "card-subinfo");
                gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(info_box), sub_lbl);

                gtk_box_append(GTK_BOX(card), info_box);

                /* 右侧操作按钮：“查看” */
                GtkWidget *btn_view = gtk_button_new_with_label("查看");
                gtk_widget_add_css_class(btn_view, "btn-action");
                gtk_widget_add_css_class(btn_view, "btn-view");
                gtk_widget_set_valign(btn_view, GTK_ALIGN_CENTER);

                /* 构建带生命周期自动清理机制的按钮上下文 */
                ViewHistoryContext *vhc = g_new0(ViewHistoryContext, 1);
                vhc->ctx = ctx;
                vhc->task_id = g_strdup(task->id);
                g_signal_connect_data(btn_view, "clicked", G_CALLBACK(on_view_history_clicked), vhc, free_view_ctx, 0);

                gtk_box_append(GTK_BOX(card), btn_view);

                /* 卡片挂载至列表容器 */
                gtk_box_append(GTK_BOX(ctx->history_list_box), card);
        }

        /* 释放任务数组外层包装 */
        g_ptr_array_unref(history);
}

/**
 * @brief 历史对话框销毁时的生命周期清理回调
 * @param widget 正在被销毁的控件指针（未使用）
 * @param user_data 关联的 HistoryDialogContext 堆内存指针
 */
static void on_destroy(GtkWidget *widget, gpointer user_data) {
        (void)widget;
        HistoryDialogContext *ctx = (HistoryDialogContext *)user_data;

        /* 
         * 触发关闭回调通知：
         * 告知调用方历史窗口已关闭，以便主窗口在必要时执行重新对齐或数据重刷
         */
        if (ctx->callback) {
                ctx->callback(ctx->user_data);
        }

        /* 释放上下文结构体 */
        g_free(ctx);
}

/**
 * @brief 构建并展示历史记录模态对话框
 * @details 创建包括顶部引导标题、空态占位文字与滚动卡片列表在内的完整窗口树。
 * @param parent 父级窗口
 * @param on_closed 关闭通知回调
 * @param user_data 传递给回调的上下文载荷
 */
void history_window_show(GtkWindow *parent, HistoryWindowClosedCallback on_closed, gpointer user_data) {
        /* 分配并零初始化对话框私有上下文 */
        HistoryDialogContext *ctx = g_new0(HistoryDialogContext, 1);
        ctx->callback = on_closed;
        ctx->user_data = user_data;

        /* 
         * 1. 顶层模态窗口构建：
         * 依附于主窗口，宽 580 高 640
         */
        GtkWidget *win = gtk_window_new();
        ctx->window = win;
        gtk_window_set_title(GTK_WINDOW(win), "历史记录");
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(win), 580, 640);
        gtk_widget_add_css_class(win, "main-window");

        /* 垂直方向根容器，四周内边距 24px */
        GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_margin_top(root_box, 24);
        gtk_widget_set_margin_bottom(root_box, 24);
        gtk_widget_set_margin_start(root_box, 24);
        gtk_widget_set_margin_end(root_box, 24);
        gtk_window_set_child(GTK_WINDOW(win), root_box);

        /* 
         * 2. 顶部标题与副标题文案
         */
        GtkWidget *title_lbl = gtk_label_new("历史记录");
        gtk_widget_add_css_class(title_lbl, "text-title");
        gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), title_lbl);

        GtkWidget *sub_lbl = gtk_label_new("已结算或已终止的板块都会显示在这里");
        gtk_widget_add_css_class(sub_lbl, "text-subtitle");
        gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), sub_lbl);

        /* 
         * 3. 空态提示标签：
         * 默认预留，当没有历史数据时由 render_history 控制显示
         */
        ctx->empty_hint_lbl = gtk_label_new("暂无历史记录～");
        gtk_widget_add_css_class(ctx->empty_hint_lbl, "empty-hint");
        gtk_widget_set_halign(ctx->empty_hint_lbl, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(root_box), ctx->empty_hint_lbl);

        /* 
         * 4. 滚动列表包裹器：
         * 填充剩余垂直空间，提供顺畅平滑的翻页浏览
         */
        GtkWidget *scroller = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(scroller, TRUE);
        ctx->history_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), ctx->history_list_box);
        gtk_box_append(GTK_BOX(root_box), scroller);

        /* 执行首次历史数据加载与渲染 */
        render_history(ctx);

        /* 绑定销毁信号回调并激活窗口 */
        g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), ctx);
        gtk_window_present(GTK_WINDOW(win));
}
