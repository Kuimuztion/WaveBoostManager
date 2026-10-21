/**
 * @file main_window.c
 * @brief 代肝任务面板 - 主工作台窗口实现
 * @details 实现了主界面的动态组装与事件流转：
 *          - 顶部一体化导航栏（品牌 Emoji、中文标题副标、历史记录/主题切换/新建任务快捷键）；
 *          - 排单卡片流（排单序号、委托人、接单时间、笔录条数动态统计）；
 *          - 快捷操作流（查看详情、弹窗二次确认结算或提前终止）；
 *          - 异步对话框状态回调与全界面数据同步。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "main_window.h"
#include "models.h"
#include "data_service.h"
#include "ui_theme.h"
#include "create_window.h"
#include "detail_window.h"
#include "history_window.h"
#include <string.h>

/**
 * @brief 主窗口私有结构体定义
 * @details 维护顶层应用窗口句柄、任务卡片垂直容器以及暂无任务时的空态提示标签。
 */
struct _MainWindow {
        GtkWidget *window;         /**< 顶层 GtkApplicationWindow 控件指针 */
        GtkWidget *task_list_box;  /**< 动态存放任务卡片的垂直容器（GtkBox） */
        GtkWidget *empty_hint_lbl; /**< 待办列表为空时的友好引导提示文字 */
};

/**
 * @brief 任务卡片快捷操作（查看、结算、终止）的事件上下文
 * @details 用于在按钮点击事件以及异步确认对话框回调之间安全传递目标任务参数。
 */
typedef struct {
        MainWindow *main_win;    /**< 归属的主窗口实例引用 */
        char *task_id;           /**< 目标任务的 32 字符 UUID 副本 */
        char *task_title;        /**< 目标任务标题副本，用于在二次确认对话框中格式化展示 */
        BoostStatus new_status;  /**< 目标状态流转（例如迁移至已结算或已终止） */
} TaskActionContext;

/**
 * @brief 子窗口（新建/详情/历史）关闭或数据变更时的同步刷新回调
 * @details 使得子模态窗口中的任何增删改操作都能即时反映在主界面看板上。
 * @param user_data 注册时传递的主窗口 MainWindow* 指针
 */
static void on_task_changed_callback(gpointer user_data) {
        MainWindow *win = (MainWindow *)user_data;
        main_window_refresh(win);
}

/**
 * @brief 导航栏“＋ 创建”按钮点击事件处理函数
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 关联的主窗口 MainWindow* 指针
 */
static void on_create_nav_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        MainWindow *win = (MainWindow *)user_data;

        /* 弹出模态新建任务对话框，并注册任务创建成功后的刷新回调 */
        create_window_show(GTK_WINDOW(win->window), on_task_changed_callback, win);
}

/**
 * @brief 导航栏“历史记录”按钮点击事件处理函数
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 关联的主窗口 MainWindow* 指针
 */
static void on_history_nav_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        MainWindow *win = (MainWindow *)user_data;

        /* 弹出历史归档记录对话框，支持追溯已完结/已终止任务的详情 */
        history_window_show(GTK_WINDOW(win->window), on_task_changed_callback, win);
}

/**
 * @brief 任务卡片“查看”按钮点击事件处理函数
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 绑定的 TaskActionContext 操作上下文
 */
static void on_view_card_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        TaskActionContext *act = (TaskActionContext *)user_data;

        /* 
         * 打开任务详情看板：
         * 第三个参数 read_only 传 FALSE，允许代肝人员查看/编辑需求并实时录入最新跟进笔录
         */
        detail_window_show(GTK_WINDOW(act->main_win->window), act->task_id, FALSE, on_task_changed_callback, act->main_win);
}

/**
 * @brief 状态流转（终止/结算）二次确认对话框的用户选择异步回调
 * @details 适配 GTK4 推荐的 GtkAlertDialog 异步交互模型。
 *          若用户点击了“确定”（索引为 0），则加载任务、修改状态、记录完结时间戳并落盘；
 *          无论用户选择确定还是取消，均安全释放上下文结构体堆内存。
 * @param source 发送异步通知的 GtkAlertDialog 对象句柄
 * @param res 异步操作结果凭据
 * @param user_data 绑定的 TaskActionContext 堆内存指针
 */
static void on_confirm_dialog_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
        GtkAlertDialog *alert = GTK_ALERT_DIALOG(source);
        TaskActionContext *act = (TaskActionContext *)user_data;
        GError *err = NULL;

        /* 获取用户选中的按钮索引：0 代表确定，1 代表取消 */
        int choice = gtk_alert_dialog_choose_finish(alert, res, &err);

        /* 用户点击了“确定”按钮 */
        if (choice == 0) {
                /* 从持久化存储中检索出该任务的最新鲜实体 */
                BoostTask *t = data_service_find_by_id(act->task_id);
                if (t) {
                        /* 
                         * 1. 更新任务生命周期状态：
                         * 将其设置为 BOOST_STATUS_TERMINATED 或 BOOST_STATUS_COMPLETED
                         */
                        t->status = act->new_status;

                        /* 
                         * 2. 赋予当前系统时间戳作为完成/终止时间：
                         * 该时间将用于后续历史归档时的倒序展示与排序依据
                         */
                        g_free(t->finished_at);
                        t->finished_at = get_current_iso_time();

                        /* 3. 落地持久化并释放临时实体 */
                        data_service_update_task(t);
                        boost_task_free(t);

                        /* 4. 即时刷新主界面，该任务卡片将从“进行中”看板中移出 */
                        main_window_refresh(act->main_win);
                }
        }

        /* 释放操作上下文中的字符串与结构体内存 */
        g_free(act->task_id);
        g_free(act->task_title);
        g_free(act);
}

/**
 * @brief 任务卡片“终止”按钮点击事件处理函数
 * @details 弹出二次确认警示对话框，防止用户在触摸屏或快速点击时误操作将任务提前取消。
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 绑定的 TaskActionContext 数据
 */
static void on_terminate_card_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        TaskActionContext *src_act = (TaskActionContext *)user_data;

        /* 
         * 构建独立的异步对话框上下文：
         * 因为对话框为非阻塞异步调用，必须持有独立的字符串副本，防止原卡片重刷销毁导致野指针
         */
        TaskActionContext *act = g_new0(TaskActionContext, 1);
        act->main_win = src_act->main_win;
        act->task_id = g_strdup(src_act->task_id);
        act->task_title = g_strdup(src_act->task_title);
        act->new_status = BOOST_STATUS_TERMINATED;

        /* 格式化二次确认文案，清晰向用户解释终止后的数据去向 */
        char *msg = g_strdup_printf("确定要终止「%s」吗？\n终止后该板块将不再显示于主界面，但可在历史记录中查看。", act->task_title);
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", msg);
        g_free(msg);

        /* 设置按钮序列与默认/取消响应 */
        gtk_alert_dialog_set_buttons(alert, (const char*[]){ "确定", "取消", NULL });
        gtk_alert_dialog_set_default_button(alert, 0);
        gtk_alert_dialog_set_cancel_button(alert, 1);

        /* 启动非阻塞异步弹窗监听 */
        gtk_alert_dialog_choose(alert, GTK_WINDOW(act->main_win->window), NULL, on_confirm_dialog_chosen, act);
        g_object_unref(alert);
}

/**
 * @brief 任务卡片“结算”按钮点击事件处理函数
 * @details 弹出结单核算确认对话框，用户确认后将任务标记为已结算并归档。
 * @param btn 触发事件的按钮指针（未使用）
 * @param user_data 绑定的 TaskActionContext 数据
 */
static void on_settle_card_clicked(GtkButton *btn, gpointer user_data) {
        (void)btn;
        TaskActionContext *src_act = (TaskActionContext *)user_data;

        /* 复制独立的异步弹窗上下文 */
        TaskActionContext *act = g_new0(TaskActionContext, 1);
        act->main_win = src_act->main_win;
        act->task_id = g_strdup(src_act->task_id);
        act->task_title = g_strdup(src_act->task_title);
        act->new_status = BOOST_STATUS_COMPLETED;

        /* 组装确认提示文案 */
        char *msg = g_strdup_printf("确认「%s」已经完成结算了吗？", act->task_title);
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", msg);
        g_free(msg);

        /* 绑定按钮与异步选择处理 */
        gtk_alert_dialog_set_buttons(alert, (const char*[]){ "确定", "取消", NULL });
        gtk_alert_dialog_set_default_button(alert, 0);
        gtk_alert_dialog_set_cancel_button(alert, 1);

        gtk_alert_dialog_choose(alert, GTK_WINDOW(act->main_win->window), NULL, on_confirm_dialog_chosen, act);
        g_object_unref(alert);
}

/**
 * @brief 卡片按钮信号绑定上下文的自动析构函数
 * @details 在卡片随列表刷新而被销毁时，GTK 自动调用本回调释放关联的上下文。
 * @param data TaskActionContext 结构体指针
 * @param closure 信号闭包指针（未使用）
 */
static void free_action_ctx(gpointer data, GClosure *closure) {
        (void)closure;
        TaskActionContext *act = (TaskActionContext *)data;
        g_free(act->task_id);
        g_free(act->task_title);
        g_free(act);
}

/**
 * @brief 重新加载并刷新主窗口待办任务看板
 * @details 从底层拉取进行中任务列表，清空当前列表控件并重新生成排单卡片流。
 * @param win 主窗口实例指针
 */
void main_window_refresh(MainWindow *win) {
        /* 
         * 1. 彻底清除旧卡片小部件：
         * 避免刷新时在原有卡片后追加重复条目
         */
        GtkWidget *child = gtk_widget_get_first_child(win->task_list_box);
        while (child) {
                GtkWidget *next = gtk_widget_get_next_sibling(child);
                gtk_box_remove(GTK_BOX(win->task_list_box), child);
                child = next;
        }

        /* 
         * 2. 加载当前处于进行中状态的任务：
         * 数据服务层已按接单时间先后完成了正序排序并预置了 rank（1, 2, 3...）
         */
        GPtrArray *in_prog = data_service_get_in_progress();

        /* 
         * 3. 空态提示控制：
         * 若当前没有任何正在进行中的委托，显示友好占位文案，引导用户点击创建
         */
        if (!in_prog || in_prog->len == 0) {
                gtk_widget_set_visible(win->empty_hint_lbl, TRUE);
                if (in_prog) g_ptr_array_unref(in_prog);
                return;
        }

        /* 存在有效进行中任务，隐藏空态标签 */
        gtk_widget_set_visible(win->empty_hint_lbl, FALSE);

        /* 
         * 4. 逐个渲染进行中任务卡片：
         * 结构三段式：[左侧序号胶囊] + [中央标题与元数据] + [右侧操作功能区]
         */
        for (guint i = 0; i < in_prog->len; i++) {
                BoostTask *task = g_ptr_array_index(in_prog, i);

                /* 卡片顶层横向容器 */
                GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
                gtk_widget_add_css_class(card, "task-card");

                /* 
                 * 4.1 左侧序号徽章（Rank Badge）：
                 * 格式如 "NO.1"，彰显接单先后的排队优先级
                 */
                char rank_text[16];
                g_snprintf(rank_text, sizeof(rank_text), "NO.%d", task->rank);
                GtkWidget *rank_badge = gtk_label_new(rank_text);
                gtk_widget_add_css_class(rank_badge, "rank-badge");
                gtk_widget_set_valign(rank_badge, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(card), rank_badge);

                /* 
                 * 4.2 中央信息区域：
                 * 包含任务大标题与副信息（委托人、接单时间、笔录条数）
                 */
                GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
                gtk_widget_set_hexpand(info_box, TRUE);
                gtk_widget_set_valign(info_box, GTK_ALIGN_CENTER);

                /* 任务大标题 */
                GtkWidget *title_lbl = gtk_label_new(task->title);
                gtk_widget_add_css_class(title_lbl, "card-title");
                gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(info_box), title_lbl);

                /* 组装元信息文本：委托人信息 + 月-日 时间 + 跟进笔录总条数 */
                const char *who = (task->requester && *task->requester) ? task->requester : "未填写委托人";
                char *created_fmt = format_display_time(task->created_at, "%m-%d %H:%M");
                guint notes_cnt = task->notes ? task->notes->len : 0;
                char *sub_info = g_strdup_printf("委托人：%s    创建于 %s    笔录 %u 条", who, created_fmt, notes_cnt);
                g_free(created_fmt);

                GtkWidget *sub_lbl = gtk_label_new(sub_info);
                g_free(sub_info);
                gtk_widget_add_css_class(sub_lbl, "card-subinfo");
                gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(info_box), sub_lbl);

                gtk_box_append(GTK_BOX(card), info_box);

                /* 
                 * 4.3 右侧操作按钮群：
                 * 包含“查看”、“终止”、“结算”三个快捷操作
                 */
                GtkWidget *actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_widget_set_valign(actions_box, GTK_ALIGN_CENTER);

                /* 【查看】按钮：打开详情与笔录编辑看板 */
                GtkWidget *btn_view = gtk_button_new_with_label("查看");
                gtk_widget_add_css_class(btn_view, "btn-action");
                gtk_widget_add_css_class(btn_view, "btn-view");
                TaskActionContext *act_view = g_new0(TaskActionContext, 1);
                act_view->main_win = win;
                act_view->task_id = g_strdup(task->id);
                act_view->task_title = g_strdup(task->title);
                g_signal_connect_data(btn_view, "clicked", G_CALLBACK(on_view_card_clicked), act_view, free_action_ctx, 0);
                gtk_box_append(GTK_BOX(actions_box), btn_view);

                /* 【终止】按钮：弹出二次确认后取消并归档 */
                GtkWidget *btn_term = gtk_button_new_with_label("终止");
                gtk_widget_add_css_class(btn_term, "btn-action");
                gtk_widget_add_css_class(btn_term, "btn-terminate");
                TaskActionContext *act_term = g_new0(TaskActionContext, 1);
                act_term->main_win = win;
                act_term->task_id = g_strdup(task->id);
                act_term->task_title = g_strdup(task->title);
                g_signal_connect_data(btn_term, "clicked", G_CALLBACK(on_terminate_card_clicked), act_term, free_action_ctx, 0);
                gtk_box_append(GTK_BOX(actions_box), btn_term);

                /* 【结算】按钮：弹出确认后顺利结单 */
                GtkWidget *btn_settle = gtk_button_new_with_label("结算");
                gtk_widget_add_css_class(btn_settle, "btn-action");
                gtk_widget_add_css_class(btn_settle, "btn-settle");
                TaskActionContext *act_settle = g_new0(TaskActionContext, 1);
                act_settle->main_win = win;
                act_settle->task_id = g_strdup(task->id);
                act_settle->task_title = g_strdup(task->title);
                g_signal_connect_data(btn_settle, "clicked", G_CALLBACK(on_settle_card_clicked), act_settle, free_action_ctx, 0);
                gtk_box_append(GTK_BOX(actions_box), btn_settle);

                gtk_box_append(GTK_BOX(card), actions_box);

                /* 卡片挂载至列表主容器 */
                gtk_box_append(GTK_BOX(win->task_list_box), card);
        }

        /* 释放任务数组外层包装 */
        g_ptr_array_unref(in_prog);
}

/**
 * @brief 构建并初始化主工作台窗口界面
 * @details 负责搭建完整的顶层主窗口，包括顶部导航工具条、居中空态提示、
 *          卡片流滚动视口，并触发首次界面数据加载。
 * @param app 所属的 GtkApplication 上下文指针
 * @return MainWindow* 主窗口封装结构体指针
 */
MainWindow *main_window_new(GtkApplication *app) {
        MainWindow *win = g_new0(MainWindow, 1);

        /* 
         * 1. 创建顶层应用窗口：
         * 设置默认尺寸 1020x740，附加 main-window 自定义全局 CSS 样式类
         */
        GtkWidget *w = gtk_application_window_new(app);
        win->window = w;
        gtk_window_set_title(GTK_WINDOW(w), "代肝任务面板");
        gtk_window_set_default_size(GTK_WINDOW(w), 1020, 740);
        gtk_widget_add_css_class(w, "main-window");

        /* 垂直方向根容器 */
        GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_window_set_child(GTK_WINDOW(w), root_box);

        /* 
         * 2. 顶部导航栏（Header Panel）：
         * 水平排列，包含左侧 Logo 与标题副标、中间弹性占位空白、右侧功能按钮组
         */
        GtkWidget *header_panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        gtk_widget_add_css_class(header_panel, "header-panel");

        /* 2.1 导航栏左侧：主副标题 */
        GtkWidget *hl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_valign(hl_box, GTK_ALIGN_CENTER);

        /* 标题与副标题垂直排版 */
        GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *h_title = gtk_label_new("代肝任务面板");
        gtk_widget_add_css_class(h_title, "text-title");
        gtk_widget_set_halign(h_title, GTK_ALIGN_START);

        GtkWidget *h_sub = gtk_label_new("进行中的委托，从上到下按接单时间排列");
        gtk_widget_add_css_class(h_sub, "text-subtitle");
        gtk_widget_set_halign(h_sub, GTK_ALIGN_START);

        gtk_box_append(GTK_BOX(title_box), h_title);
        gtk_box_append(GTK_BOX(title_box), h_sub);
        gtk_box_append(GTK_BOX(hl_box), title_box);

        gtk_box_append(GTK_BOX(header_panel), hl_box);

        /* 2.2 弹性占位控件（将右侧按钮群推至视口最右边） */
        GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(spacer, TRUE);
        gtk_box_append(GTK_BOX(header_panel), spacer);

        /* 2.3 导航栏右侧功能按钮组：历史记录、新建任务 */
        GtkWidget *hr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_valign(hr_box, GTK_ALIGN_CENTER);

        /* 历史记录按钮 */
        GtkWidget *btn_hist = gtk_button_new_with_label("历史记录");
        gtk_widget_add_css_class(btn_hist, "btn-nav");
        g_signal_connect(btn_hist, "clicked",
                         G_CALLBACK(on_history_nav_clicked), win);
        gtk_box_append(GTK_BOX(hr_box), btn_hist);

        /* 新建委托板块主要行动按钮 */
        GtkWidget *btn_create = gtk_button_new_with_label("创建");
        gtk_widget_add_css_class(btn_create, "btn-create-nav");
        g_signal_connect(btn_create, "clicked",
                         G_CALLBACK(on_create_nav_clicked), win);
        gtk_box_append(GTK_BOX(hr_box), btn_create);

        gtk_box_append(GTK_BOX(header_panel), hr_box);
        gtk_box_append(GTK_BOX(root_box), header_panel);

        /* 
         * 3. 主视口内容区：
         * 包含无任务时的空态占位标签及可滚动的卡片列表
         */
        /* 空态占位文案 */
        win->empty_hint_lbl = gtk_label_new("暂无进行中的代肝任务，点击右上角「创建」添加一个吧～");
        gtk_widget_add_css_class(win->empty_hint_lbl, "empty-hint");
        gtk_widget_set_halign(win->empty_hint_lbl, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(root_box), win->empty_hint_lbl);

        /* 滚动容器 */
        GtkWidget *scroller = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(scroller, TRUE);
        gtk_widget_set_margin_top(scroller, 20);
        gtk_widget_set_margin_bottom(scroller, 20);
        gtk_widget_set_margin_start(scroller, 24);
        gtk_widget_set_margin_end(scroller, 24);

        win->task_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), win->task_list_box);
        gtk_box_append(GTK_BOX(root_box), scroller);

        /* 4. 执行初次看板数据刷新 */
        main_window_refresh(win);

        return win;
}

/**
 * @brief 获取主工作台底层的顶层 GtkWindow 句柄
 * @param win 主窗口结构体指针
 * @return GtkWindow* 顶层窗口部件指针
 */
GtkWindow *main_window_get_window(MainWindow *win) {
        return GTK_WINDOW(win->window);
}
