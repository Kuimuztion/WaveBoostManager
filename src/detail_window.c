/**
 * @file detail_window.c
 * @brief 代肝任务面板 - 板块详情与跟进笔录对话框实现
 * @details 实现了查看和编辑任务需求描述、按时间倒序动态渲染跟进笔录卡片列表、
 *          实时录入新笔录、以及历史已归档任务自动只读保护等核心业务交互。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "detail_window.h"
#include "models.h"
#include "data_service.h"
#include <string.h>

/**
 * @brief 详情对话框私有运行上下文
 * @details 保存当前正在展示或编辑的任务对象镜像、输入控件句柄以及生命周期相关回调。
 */
typedef struct {
        GtkWidget *window;             /**< 对话框顶层窗口 */
        char *task_id;                 /**< 正在查看的任务 32 字符 ID */
        gboolean read_only;            /**< 只读标志：为 TRUE 时禁用编辑与新增笔录 */
        BoostTask *task;               /**< 内存中持有的任务对象当前镜像 */
        GtkWidget *notes_list_box;     /**< 动态承载所有笔录卡片的容器（GtkBox） */
        GtkWidget *entry_new_note;     /**< 新增笔录文本输入框（仅非只读模式下存在） */
        GtkWidget *tv_desc;            /**< 需求描述多行文本框 */
        TaskUpdatedCallback callback;  /**< 数据更新后的外层刷新回调 */
        gpointer user_data;            /**< 回调函数附带的上下文数据 */
} DetailDialogContext;

/**
 * @brief 重新渲染跟进笔录列表视图
 * @details 清除当前容器内所有旧子项，并按时间倒序（最新追加的在最上方）
 *          将笔录逐条构筑为带时间戳的卡片插入容器；若为空则显示占位提示。
 * @param ctx 详情对话框上下文
 */
static void render_notes(DetailDialogContext *ctx) {
        /* 
         * 1. 清空当前容器内的所有现有子控件：
         * GTK4 推荐通过遍历 first_child -> next_sibling 进行安全逐一移除
         */
        GtkWidget *child = gtk_widget_get_first_child(ctx->notes_list_box);
        while (child) {
                GtkWidget *next = gtk_widget_get_next_sibling(child);
                gtk_box_remove(GTK_BOX(ctx->notes_list_box), child);
                child = next;
        }

        /* 
         * 2. 空列表兜底防护：
         * 若任务没有笔录或笔录数组长度为 0，展示灰色友好的空态占位文字
         */
        if (!ctx->task || !ctx->task->notes || ctx->task->notes->len == 0) {
                GtkWidget *empty_lbl = gtk_label_new("暂无跟进笔录");
                gtk_widget_add_css_class(empty_lbl, "empty-hint");
                gtk_box_append(GTK_BOX(ctx->notes_list_box), empty_lbl);
                return;
        }

        /* 
         * 3. 倒序遍历渲染笔录（从最后一条到第一条）：
         * 业务考量：在代肝跟进场景中，用户最关心的是最新的进展和刚发生的状态，
         * 将最新笔录置于顶部可极大降低用户的翻阅滚动成本。
         */
        for (int i = (int)ctx->task->notes->len - 1; i >= 0; i--) {
                NoteEntry *ne = g_ptr_array_index(ctx->task->notes, i);

                /* 创建单条笔录卡片外层容器 */
                GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
                gtk_widget_add_css_class(card, "note-card");

                /* 上行：记录时间戳（年-月-日 时:分），应用副标题淡色样式 */
                char *time_display = format_display_time(ne->time, "%Y-%m-%d %H:%M");
                GtkWidget *time_lbl = gtk_label_new(time_display);
                g_free(time_display);
                gtk_widget_add_css_class(time_lbl, "text-subtitle");
                gtk_widget_set_halign(time_lbl, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(card), time_lbl);

                /* 下行：笔录正文，开启自动折行，保证长文本不溢出裁剪 */
                GtkWidget *cont_lbl = gtk_label_new(ne->content);
                gtk_widget_add_css_class(cont_lbl, "text-primary");
                gtk_label_set_wrap(GTK_LABEL(cont_lbl), TRUE);
                gtk_label_set_wrap_mode(GTK_LABEL(cont_lbl), PANGO_WRAP_WORD_CHAR);
                gtk_widget_set_halign(cont_lbl, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(card), cont_lbl);

                /* 追加至笔录列表容器 */
                gtk_box_append(GTK_BOX(ctx->notes_list_box), card);
        }
}

/**
 * @brief “添加笔录”确认按钮或回车激活事件回调函数
 * @details 获取并修剪输入框文本，向任务内存模型追加新笔录并即时持久化到磁盘，
 *          随后刷新界面列表，清空输入框，并触发主窗口更新。
 * @param btn 触发事件的按钮指针（或激活事件发送者）
 * @param user_data 关联的 DetailDialogContext 上下文指针
 */
static void on_add_note_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        DetailDialogContext *ctx = (DetailDialogContext *)user_data;

        /* 获取输入框文本并剔除前后无效空格 */
        const char *raw = gtk_editable_get_text(GTK_EDITABLE(ctx->entry_new_note));
        char *text = g_strstrip(g_strdup(raw));

        /* 卫语句：严禁提交空白笔录 */
        if (!text || strlen(text) == 0) {
                g_free(text);
                return;
        }

        /* 
         * 1. 向任务内存镜像中追加笔录：
         * 内部自动获取当前系统时间作为该笔录的时间戳
         */
        boost_task_add_note(ctx->task, text);

        /* 2. 将包含新笔录的任务实体全量同步持久化至磁盘 tasks.json */
        data_service_update_task(ctx->task);
        g_free(text);

        /* 3. 清空输入框内容，方便用户直接开始录入下一条 */
        gtk_editable_set_text(GTK_EDITABLE(ctx->entry_new_note), "");

        /* 4. 即时重绘刷新对话框内的笔录列表卡片 */
        render_notes(ctx);

        /* 5. 触发外部回调，通知主窗口同步更新“笔录 N 条”计数 */
        if (ctx->callback) {
                ctx->callback(ctx->user_data);
        }
}

/**
 * @brief “保存修改”按钮点击事件回调函数
 * @details 采集用户在多行文本框中最新修改的需求描述并保存至存储系统。
 * @param btn 按钮句柄（未使用）
 * @param user_data 关联的 DetailDialogContext 上下文指针
 */
static void on_save_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        DetailDialogContext *ctx = (DetailDialogContext *)user_data;

        /* 提取 TextBuffer 内所有文本内容并修剪两端空白 */
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->tv_desc));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buf, &start, &end);
        char *raw = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
        char *trimmed = g_strstrip(g_strdup(raw));
        g_free(raw);

        /* 更新任务对象内部字段 */
        g_free(ctx->task->description);
        ctx->task->description = trimmed;

        /* 同步写入磁盘存储 */
        data_service_update_task(ctx->task);

        /* 通知主窗口刷新缓存 */
        if (ctx->callback)
                ctx->callback(ctx->user_data);
}

/**
 * @brief “关闭”按钮点击事件回调函数
 * @param btn 按钮句柄（未使用）
 * @param user_data 关联的 DetailDialogContext 上下文指针
 */
static void on_close_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        DetailDialogContext *ctx = (DetailDialogContext *)user_data;

        /* 触发窗口关闭销毁流程 */
        gtk_window_destroy(GTK_WINDOW(ctx->window));
}

/**
 * @brief 窗口销毁时清理上下文资源的生命周期回调
 * @param widget 正在被销毁的控件指针（未使用）
 * @param user_data DetailDialogContext 指针
 */
static void on_destroy(GtkWidget *widget, gpointer user_data) {
        (void)widget;
        DetailDialogContext *ctx = (DetailDialogContext *)user_data;

        /* 释放持有的任务对象实体镜像 */
        if (ctx->task)
                boost_task_free(ctx->task);

        /* 释放任务 ID 字符串及上下文结构体自身 */
        g_free(ctx->task_id);
        g_free(ctx);
}

/**
 * @brief 构建并展示任务详情与跟进对话框
 * @details 负责加载指定 ID 的任务数据，构建包括头部标题、状态胶囊、元数据、
 *          需求富文本框、笔录滚动列表以及操作栏在内的完备界面。
 * @param parent 父级窗口
 * @param task_id 目标任务的 32 字符 UUID
 * @param read_only 是否以只读方式展示
 * @param on_updated 更新完成后的回调
 * @param user_data 传递给回调的上下文载荷
 */
void detail_window_show(GtkWindow *parent, const char *task_id, gboolean read_only, TaskUpdatedCallback on_updated, gpointer user_data) {
        /* 
         * 1. 从数据持久层加载目标任务的独立实体：
         * 若未找到（可能已被其他窗口终止或删除），给出提示并拦截流程
         */
        BoostTask *task = data_service_find_by_id(task_id);
        if (!task) {
                GtkAlertDialog *alert = gtk_alert_dialog_new("%s", "未找到该板块，可能已被删除。");
                gtk_alert_dialog_show(alert, parent);
                g_object_unref(alert);
                return;
        }

        /* 分配并初始化运行上下文 */
        DetailDialogContext *ctx = g_new0(DetailDialogContext, 1);
        ctx->task_id = g_strdup(task_id);
        ctx->task = task;
        /* 
         * 业务规则：只要外部指定了只读，或者任务自身已不是“进行中”（即已结算或已终止），
         * 均自动归入只读模式，锁定需求编辑与追加笔录功能，防止误改历史归档数据。
         */
        ctx->read_only = read_only || (task->status != BOOST_STATUS_IN_PROGRESS);
        ctx->callback = on_updated;
        ctx->user_data = user_data;

        /* 
         * 2. 构建模态顶层窗口：
         * 设置固定比例尺寸 560x660，支持居中弹出
         */
        GtkWidget *win = gtk_window_new();
        ctx->window = win;
        gtk_window_set_title(GTK_WINDOW(win), "板块详情");
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(win), 560, 660);
        gtk_widget_add_css_class(win, "main-window");

        /* 垂直根布局容器，四周内边距 24px */
        GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_set_margin_top(root_box, 24);
        gtk_widget_set_margin_bottom(root_box, 24);
        gtk_widget_set_margin_start(root_box, 24);
        gtk_widget_set_margin_end(root_box, 24);
        gtk_window_set_child(GTK_WINDOW(win), root_box);

        /* 
         * 3. 头部信息：板块标题 + 状态胶囊标签（Pill Badge）
         */
        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *title_lbl = gtk_label_new(task->title);
        gtk_widget_add_css_class(title_lbl, "text-title");
        gtk_box_append(GTK_BOX(header_box), title_lbl);

        /* 根据任务状态附加对应的徽章背景与文字颜色 CSS 类 */
        const char *status_str = boost_status_to_string(task->status);
        GtkWidget *status_badge = gtk_label_new(status_str);
        gtk_widget_add_css_class(status_badge, "status-badge");
        if (task->status == BOOST_STATUS_COMPLETED) {
                gtk_widget_add_css_class(status_badge, "status-completed");
        } else if (task->status == BOOST_STATUS_TERMINATED) {
                gtk_widget_add_css_class(status_badge, "status-terminated");
        } else {
                gtk_widget_add_css_class(status_badge, "status-progress");
        }
        gtk_box_append(GTK_BOX(header_box), status_badge);
        gtk_box_append(GTK_BOX(root_box), header_box);

        /* 
         * 4. 任务元信息（委托人、接单时间、结单时间）：
         * 采用中圆点排版风格，呈现优雅紧凑的视觉体验
         */
        const char *who = (task->requester && *task->requester) ? task->requester : "未填写";
        char *created_fmt = format_display_time(task->created_at, "%Y-%m-%d %H:%M");
        GString *meta_str = g_string_new("");
        g_string_printf(meta_str, "委托人：%s　·　创建于 %s", who, created_fmt);
        g_free(created_fmt);

        /* 若已结单或已终止，追加显示完结时间点 */
        if (task->finished_at && *task->finished_at) {
                char *fin_fmt = format_display_time(task->finished_at, "%Y-%m-%d %H:%M");
                const char *verb = (task->status == BOOST_STATUS_COMPLETED) ? "结算" : "终止";
                g_string_append_printf(meta_str, "　·　%s于 %s", verb, fin_fmt);
                g_free(fin_fmt);
        }

        GtkWidget *meta_lbl = gtk_label_new(meta_str->str);
        g_string_free(meta_str, TRUE);
        gtk_widget_add_css_class(meta_lbl, "text-subtitle");
        gtk_widget_set_halign(meta_lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), meta_lbl);

        /* 
         * 5. 需求描述区域：
         * 包含字段标签及带滚动条的多行文本框（只读模式下禁用编辑）
         */
        GtkWidget *desc_header = gtk_label_new("需求描述");
        gtk_widget_add_css_class(desc_header, "field-label");
        gtk_widget_set_halign(desc_header, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), desc_header);

        GtkWidget *desc_scroller = gtk_scrolled_window_new();
        gtk_widget_set_size_request(desc_scroller, -1, 90); /* 固定适中高度，留出空间给笔录列表 */
        ctx->tv_desc = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->tv_desc), GTK_WRAP_WORD_CHAR);
        gtk_widget_add_css_class(ctx->tv_desc, "custom-textview");
        if (ctx->read_only) {
                gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->tv_desc), FALSE);
        }
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->tv_desc));
        gtk_text_buffer_set_text(buf, task->description ? task->description : "", -1);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(desc_scroller), ctx->tv_desc);
        gtk_box_append(GTK_BOX(root_box), desc_scroller);

        /* 
         * 6. 跟进笔录展示区域：
         * 占据主要纵向扩展空间，支持流畅的鼠标滚轮与手势滚动
         */
        GtkWidget *notes_header = gtk_label_new("跟进笔录");
        gtk_widget_add_css_class(notes_header, "field-label");
        gtk_widget_set_halign(notes_header, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), notes_header);

        GtkWidget *notes_scroller = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(notes_scroller, TRUE);
        GtkWidget *notes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(notes_box, "notes-box");

        ctx->notes_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_box_append(GTK_BOX(notes_box), ctx->notes_list_box);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroller), notes_box);
        gtk_box_append(GTK_BOX(root_box), notes_scroller);

        /* 初始化笔录卡片渲染 */
        render_notes(ctx);

        /* 
         * 7. 录入新笔录输入面板：
         * 仅在进行中（非只读模式）展示；支持输入回车直接快捷添加
         */
        if (!ctx->read_only) {
                GtkWidget *add_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                ctx->entry_new_note = gtk_entry_new();
                gtk_widget_add_css_class(ctx->entry_new_note, "custom-entry");
                gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->entry_new_note), "添加跟进笔录内容...");
                gtk_widget_set_hexpand(ctx->entry_new_note, TRUE);
                /* 绑定回车快捷触发添加 */
                g_signal_connect_swapped(ctx->entry_new_note, "activate", G_CALLBACK(on_add_note_clicked), ctx);

                GtkWidget *btn_add = gtk_button_new_with_label("添加笔录");
                gtk_widget_add_css_class(btn_add, "btn-primary");
                g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_note_clicked), ctx);

                gtk_box_append(GTK_BOX(add_box), ctx->entry_new_note);
                gtk_box_append(GTK_BOX(add_box), btn_add);
                gtk_box_append(GTK_BOX(root_box), add_box);
        }

        /* 
         * 8. 底部操作按钮栏：
         * 右对齐，包含“关闭”以及可编辑模式下的“保存修改”
         */
        GtkWidget *btn_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_halign(btn_bar, GTK_ALIGN_END);

        GtkWidget *btn_close = gtk_button_new_with_label("关闭");
        gtk_widget_add_css_class(btn_close, "btn-secondary");
        g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), ctx);
        gtk_box_append(GTK_BOX(btn_bar), btn_close);

        if (!ctx->read_only) {
                GtkWidget *btn_save = gtk_button_new_with_label("保存修改");
                gtk_widget_add_css_class(btn_save, "btn-primary");
                g_signal_connect(btn_save, "clicked",
                                 G_CALLBACK(on_save_clicked), ctx);
                gtk_box_append(GTK_BOX(btn_bar), btn_save);
        }

        gtk_box_append(GTK_BOX(root_box), btn_bar);

        /* 绑定窗口销毁回收回调并呈现窗口 */
        g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), ctx);
        gtk_window_present(GTK_WINDOW(win));
}
