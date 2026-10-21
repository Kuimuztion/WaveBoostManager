/**
 * @file create_window.c
 * @brief 代肝任务面板 - 新建板块（代肝委托）对话框实现
 * @details 实现了新建代肝任务的用户交互窗口。采集用户输入的任务板块标题、号主联系方式
 *          及初始详细需求，自动提取并生成首条初始笔录，保证代肝全流程可溯源。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "create_window.h"
#include "models.h"
#include "data_service.h"
#include <string.h>

/**
 * @brief 创建对话框的私有上下文结构体
 * @details 保存对话框窗口自身句柄、各输入框控件指针以及完成回调上下文，
 *          供按钮点击事件处理函数访问；并在窗口销毁时一并释放。
 */
typedef struct {
        GtkWidget *window;             /**< 对话框顶层窗口对象 */
        GtkWidget *entry_title;        /**< 任务标题单行输入框 */
        GtkWidget *entry_requester;    /**< 委托人/号主备注单行输入框 */
        GtkWidget *tv_desc;            /**< 详细需求多行富文本输入控件 */
        TaskCreatedCallback callback;  /**< 任务落盘成功后的通知回调指针 */
        gpointer user_data;            /**< 回调函数附带的上下文数据 */
} CreateDialogContext;

/**
 * @brief “取消”按钮点击事件回调函数
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 关联的 CreateDialogContext 上下文指针
 */
static void on_cancel_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn; /* 显式静默未使用的参数编译警告 */
        CreateDialogContext *ctx = (CreateDialogContext *)user_data;

        /* 直接销毁当前对话框窗口，触发 "destroy" 信号并回收资源 */
        gtk_window_destroy(GTK_WINDOW(ctx->window));
}

/**
 * @brief “创建板块”确认按钮点击事件回调函数
 * @details 负责校验必填项、收集表单字段、自动将初始需求记录为首条跟进笔录、
 *          调用数据层入库，并触发上层回调以无缝刷新主界面。
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 关联的 CreateDialogContext 上下文指针
 */
static void on_create_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        CreateDialogContext *ctx = (CreateDialogContext *)user_data;

        /* 
         * 1. 采集并校验板块标题（必填项）：
         * 使用 g_strstrip 原地去除用户输入的首尾空白字符。
         * 若标题为空，使用 GTK4 原生非阻塞 GtkAlertDialog 给予阻断提示，避免产生无名任务。
         */
        const char *raw_title = gtk_editable_get_text(GTK_EDITABLE(ctx->entry_title));
        char *title = g_strstrip(g_strdup(raw_title));
        if (!title || strlen(title) == 0) {
                g_free(title);
                GtkAlertDialog *alert = gtk_alert_dialog_new("%s", "请填写板块标题～");
                gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->window));
                g_object_unref(alert);
                return;
        }

        /* 
         * 2. 采集委托人信息（选填项）：
         * 同样执行首尾修剪，即便为空也不会阻断流程
         */
        const char *raw_req = gtk_editable_get_text(GTK_EDITABLE(ctx->entry_requester));
        char *requester = g_strstrip(g_strdup(raw_req));

        /* 
         * 3. 从 GtkTextView 的关联 TextBuffer 中提取详细需求文本：
         * 必须获取缓冲区的起止迭代器 bounds 才能准确提取全部输入内容
         */
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->tv_desc));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buf, &start, &end);
        char *raw_desc = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
        char *desc = g_strstrip(g_strdup(raw_desc));
        g_free(raw_desc); /* 释放原始临时提取缓冲区 */

        /* 
         * 4. 实例化新的任务实体：
         * 内部会自动生成 32 字符十六进制 UUID 并设置初始状态为 BOOST_STATUS_IN_PROGRESS
         */
        BoostTask *task = boost_task_new(title, requester, desc);

        /* 
         * 5. 自动沉淀首条跟进笔录：
         * 业务规范：若创建时填写了详细需求，则自动格式化追加一条带有【创建时需求】前缀的笔录，
         * 方便代肝人员后续在详情看板中追溯初始约定，防止客户中途更改需求引起纠纷。
         */
        if (desc && strlen(desc) > 0) {
                char *init_note = g_strdup_printf("【创建时需求】%s", desc);
                boost_task_add_note(task, init_note);
                g_free(init_note);
        }

        /* 6. 持久化存储至 tasks.json */
        data_service_add_task(task);

        /* 释放任务临时实例及本地收集的字符串 */
        boost_task_free(task);
        g_free(title);
        g_free(requester);
        g_free(desc);

        /* 
         * 7. 触发成功回调：
         * 通知主窗口执行 main_window_refresh()，以便新板块即时展示在列表中
         */
        if (ctx->callback) {
                ctx->callback(ctx->user_data);
        }

        /* 8. 关闭并销毁创建窗口 */
        gtk_window_destroy(GTK_WINDOW(ctx->window));
}

/**
 * @brief 窗口销毁时清理上下文的生命周期回调
 * @param widget 正在被销毁的窗口小部件（未使用）
 * @param user_data 关联的 CreateDialogContext 堆内存指针
 */
static void on_destroy(GtkWidget *widget, gpointer user_data) {
        (void)widget;
        CreateDialogContext *ctx = (CreateDialogContext *)user_data;

        /* 释放由 create_window_show 动态分配的上下文结构体本身 */
        g_free(ctx);
}

/**
 * @brief 构建并显示“创建新板块”模态对话框
 * @details 负责构建完备的 GTK4 界面控件树，包括标题、输入框、滚动多行文本区域及底部操作栏。
 * @param parent 父级窗口句柄，确立模态遮罩依赖
 * @param on_created 任务创建成功后的回调
 * @param user_data 传递给回调的上下文载荷
 */
void create_window_show(GtkWindow *parent, TaskCreatedCallback on_created, gpointer user_data) {
        /* 分配并零初始化对话框私有上下文 */
        CreateDialogContext *ctx = g_new0(CreateDialogContext, 1);
        ctx->callback = on_created;
        ctx->user_data = user_data;

        /* 
         * 1. 顶层窗口属性设置：
         * 设定标题、依附父窗口、开启模态交互模式（阻断父级点击）、设定默认尺寸与样式类名
         */
        GtkWidget *win = gtk_window_new();
        ctx->window = win;
        gtk_window_set_title(GTK_WINDOW(win), "创建新板块");
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(win), 480, 520);
        gtk_widget_add_css_class(win, "main-window");

        /* 
         * 2. 根容器布局：
         * 采用垂直方向排列的 GtkBox，间距为 16px，统一设置 28px 的四周外边距
         */
        GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
        gtk_widget_set_margin_top(root_box, 28);
        gtk_widget_set_margin_bottom(root_box, 28);
        gtk_widget_set_margin_start(root_box, 28);
        gtk_widget_set_margin_end(root_box, 28);
        gtk_window_set_child(GTK_WINDOW(win), root_box);

        /* 
         * 3. 头部大标题：
         * 左对齐，应用 text-title CSS 类
         */
        GtkWidget *title_lbl = gtk_label_new("创建新板块");
        gtk_widget_add_css_class(title_lbl, "text-title");
        gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root_box), title_lbl);

        /* 
         * 4. 表单字段一：板块标题输入框
         */
        GtkWidget *box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget *lbl1 = gtk_label_new("板块标题（例如：肝200抽）");
        gtk_widget_add_css_class(lbl1, "field-label");
        gtk_widget_set_halign(lbl1, GTK_ALIGN_START);
        ctx->entry_title = gtk_entry_new();
        gtk_widget_add_css_class(ctx->entry_title, "custom-entry");
        gtk_box_append(GTK_BOX(box1), lbl1);
        gtk_box_append(GTK_BOX(box1), ctx->entry_title);
        gtk_box_append(GTK_BOX(root_box), box1);

        /* 
         * 5. 表单字段二：委托人 / 号主信息输入框（可选）
         */
        GtkWidget *box2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget *lbl2 = gtk_label_new("委托人 / 号主信息（联系方式、备注名等，可选）");
        gtk_widget_add_css_class(lbl2, "field-label");
        gtk_widget_set_halign(lbl2, GTK_ALIGN_START);
        ctx->entry_requester = gtk_entry_new();
        gtk_widget_add_css_class(ctx->entry_requester, "custom-entry");
        gtk_box_append(GTK_BOX(box2), lbl2);
        gtk_box_append(GTK_BOX(box2), ctx->entry_requester);
        gtk_box_append(GTK_BOX(root_box), box2);

        /* 
         * 6. 表单字段三：详细需求 / 初始笔录（可拉伸的多行文本框）
         */
        GtkWidget *box3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_vexpand(box3, TRUE); /* 占据剩余垂直扩展空间 */
        GtkWidget *lbl3 = gtk_label_new("详细需求 / 初始笔录");
        gtk_widget_add_css_class(lbl3, "field-label");
        gtk_widget_set_halign(lbl3, GTK_ALIGN_START);

        /* 滚动包裹容器，避免文本过长撑破窗口 */
        GtkWidget *scrolled = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(scrolled, TRUE);
        ctx->tv_desc = gtk_text_view_new();
        /* 按字符/单词进行友好折行 */
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->tv_desc), GTK_WRAP_WORD_CHAR);
        gtk_widget_add_css_class(ctx->tv_desc, "custom-textview");
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), ctx->tv_desc);

        gtk_box_append(GTK_BOX(box3), lbl3);
        gtk_box_append(GTK_BOX(box3), scrolled);
        gtk_box_append(GTK_BOX(root_box), box3);

        /* 
         * 7. 底部按钮控制栏（取消 + 创建）
         */
        GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_halign(btn_box, GTK_ALIGN_END); /* 靠右对齐 */

        GtkWidget *btn_cancel = gtk_button_new_with_label("取消");
        gtk_widget_add_css_class(btn_cancel, "btn-secondary");
        g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_clicked), ctx);
        gtk_box_append(GTK_BOX(btn_box), btn_cancel);

        GtkWidget *btn_create = gtk_button_new_with_label("创建板块");
        gtk_widget_add_css_class(btn_create, "btn-primary");
        g_signal_connect(btn_create, "clicked", G_CALLBACK(on_create_clicked), ctx);
        gtk_box_append(GTK_BOX(btn_box), btn_create);

        gtk_box_append(GTK_BOX(root_box), btn_box);

        /* 
         * 8. 窗口销毁与展示：
         * 监听窗口销毁事件以安全回收 ctx 上下文内存
         */
        g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), ctx);
        gtk_window_present(GTK_WINDOW(win));
}
