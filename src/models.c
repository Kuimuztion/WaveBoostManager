/**
 * @file models.c
 * @brief 代肝任务面板 - 核心数据模型与基础工具实现
 * @details 实现了代肝委托任务及跟进笔录的内存生命周期管理、UUID无横线格式生成、
 *          高鲁棒性的 ISO-8601 时间解析与展示格式化、以及中英文/数值状态兼容映射。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 创建新的笔录条目对象并初始化内容与记录时间
 * @param content 笔录文本正文，如果为空则兜底为空串，保证字段始终非空，避免调用端出现段错误
 * @param iso_time 显式指定的时间戳（用于反序列化历史记录）；若未提供则默认采样当前系统时间
 * @return NoteEntry* 堆内存中新建的条目指针
 */
NoteEntry *note_entry_new(const char *content, const char *iso_time) {
        /* 分配并零初始化结构体内存，确保未明确初始化的指针成员均为 NULL */
        NoteEntry *entry = g_new0(NoteEntry, 1);

        /* 
         * 针对内容进行安全复制：
         * 若 content 为 NULL，则赋空字符串 ""，防止后续上层界面取值打印时发生空指针解引用。
         */
        entry->content = g_strdup(content ? content : "");

        /* 
         * 判定时间戳来源：
         * 若调用方显式提供了有效非空的 iso_time（如数据反序列化场景），则直接复制保留原历史时间；
         * 否则属于用户即时新增笔录场景，自动生成当前系统的本地 ISO-8601 时间戳。
         */
        if (iso_time && *iso_time) {
                entry->time = g_strdup(iso_time);
        } else {
                entry->time = get_current_iso_time();
        }

        return entry;
}

/**
 * @brief 释放笔录条目内存及内部动态字符串
 * @param note 待释放的笔录条目指针，允许为 NULL（卫语句处理）
 */
void note_entry_free(NoteEntry *note) {
        /* 卫语句：若传入空指针则直接返回，避免无意义解引用或重复释放风险 */
        if (!note) return;

        /* 依次释放动态分配的时间戳与内容缓冲区 */
        g_free(note->time);
        g_free(note->content);

        /* 释放外层结构体实例本身 */
        g_free(note);
}

/**
 * @brief 生成符合 C# Guid "N" 格式的 32 字符十六进制唯一标识符
 * @details GLib 的 g_uuid_string_random() 默认返回标准 RFC 4122 格式（如 "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"），
 *          包含 4 个连字符（总长 36 字节）。为了保证与前身 C# 桌面端导出的 tasks.json 数据的严格双向兼容性，
 *          本函数原地消除所有连字符，提取连续紧凑的 32 字节十六进制标识。
 * @return char* 动态分配的 32 位 UUID 字符串，调用方负责后续释放
 */
static char *generate_task_id(void) {
        /* 生成标准的带连字符的随机 UUID 字符串 */
        char *uuid = g_uuid_string_random();

        /* 
         * 双指针原地压实去横杠：
         * p 为读取指针遍历原字符串，dst 为写入指针记录有效十六进制字符。
         * 因为去横杠后的长度只减不增，原地修改完全内存安全，无需进行二次内存分配。
         */
        char *p = uuid;
        char *dst = uuid;
        while (*p) {
                if (*p != '-') {
                        *dst++ = *p;
                }
                p++;
        }
        /* 追加字符串终止符截断尾部 */
        *dst = '\0';

        return uuid;
}

/**
 * @brief 创建并初始化一个新的代肝任务
 * @param title 任务标题（如目标委托描述）
 * @param requester 委托人账号或联系方式
 * @param description 需求细化说明
 * @return BoostTask* 新任务指针，分配独立 32 位 ID，初始状态为进行中，附带自动释放回收机制的笔录列表
 */
BoostTask *boost_task_new(const char *title, const char *requester, const char *description) {
        /* 申请内存并清零，防止结构体字段产生随机未初始化指针 */
        BoostTask *task = g_new0(BoostTask, 1);

        /* 生成无连字符的全局唯一 32 字符 ID */
        task->id = generate_task_id();

        /* 
         * 字符串字段安全深拷贝并兜底空指针：
         * 确保所有字符型成员在任何情况下都是有效的 C 字符串，降低后续界面绘制的判空成本。
         */
        task->title = g_strdup(title ? title : "");
        task->requester = g_strdup(requester ? requester : "");
        task->description = g_strdup(description ? description : "");

        /* 创建时间记录为当前系统即时时间戳 */
        task->created_at = get_current_iso_time();

        /* 新建委托默认尚未完结，完成时间置为空 */
        task->finished_at = NULL;

        /* 初始状态统一为“进行中” */
        task->status = BOOST_STATUS_IN_PROGRESS;

        /* 
         * 初始化笔录动态指针数组：
         * 绑定 note_entry_free 作为数组析构函数，在调用 g_ptr_array_unref 时自动级联释放每个元素，
         * 杜绝笔录条目的内存泄漏。
         */
        task->notes = g_ptr_array_new_with_free_func((GDestroyNotify)note_entry_free);

        /* 序号默认置为 0，后续由展示层查询计算后动态赋予 */
        task->rank = 0;

        return task;
}

/**
 * @brief 释放代肝任务所持有的所有动态资源
 * @param task 待释放的任务结构体指针
 */
void boost_task_free(BoostTask *task) {
        /* 卫语句防护：空指针直接安全退出 */
        if (!task) return;

        /* 释放所有持有的动态分配字符串 */
        g_free(task->id);
        g_free(task->title);
        g_free(task->requester);
        g_free(task->description);
        g_free(task->created_at);
        g_free(task->finished_at);

        /* 
         * 解引用笔录容器：
         * 会自动触发创建时注册的析构回调函数，逐一释放每一条 NoteEntry 结构及其动态内部字段。
         */
        if (task->notes) {
                g_ptr_array_unref(task->notes);
        }

        /* 最后释放任务结构体自身 */
        g_free(task);
}

/**
 * @brief 为任务实例追加一条新的跟进笔录
 * @param task 目标任务
 * @param content 笔录文本内容
 */
void boost_task_add_note(BoostTask *task, const char *content) {
        /* 严格参数验证：任务无效或笔录容器未初始化时直接拦截 */
        if (!task || !task->notes) return;

        /* 
         * 创建新的笔录条目：
         * 传 NULL 表示采用当前系统时间作为该笔录的时间戳。
         */
        NoteEntry *entry = note_entry_new(content, NULL);

        /* 将笔录对象的所有权移交给 GPtrArray 管理 */
        g_ptr_array_add(task->notes, entry);
}

/**
 * @brief 获取当前系统本地时间的 ISO-8601 标准字符串表示
 * @return char* 格式为 "YYYY-MM-DDTHH:MM:SS" 的字符串副本，需由调用者 g_free()
 */
char *get_current_iso_time(void) {
        /* 获取当前系统的本地时区时间对象 */
        GDateTime *now = g_date_time_new_now_local();

        /* 格式化为国际标准 ISO-8601 格式，日期与时间中间用 'T' 隔开 */
        char *str = g_date_time_format(now, "%Y-%m-%dT%H:%M:%S");

        /* 释放 GDateTime 句柄，防止系统时间上下文泄漏 */
        g_date_time_unref(now);

        return str;
}

/**
 * @brief 将 ISO-8601 字符串解析并转换为指定模板的可读界面文本
 * @details 为防止历史数据中存在格式微差（例如日期与时间之间使用空格而非 'T'），
 *          本函数内置了“标准解析 -> 空格替换容错解析 -> 纯字符串子串切片”的三级回退防护机制，
 *          确保即使用户手动修改配置文件损坏了时间精度，UI 界面也不会崩溃或显示空白。
 * @param iso_time 原始时间戳文本
 * @param format_str 目标 strftime 格式化串（如 "%Y-%m-%d %H:%M"）
 * @return char* 格式化后的时间文本，需使用 g_free() 释放
 */
char *format_display_time(const char *iso_time, const char *format_str) {
        /* 阶段一：防御性判空，空串或 NULL 直接返回空白字符串 */
        if (!iso_time || !*iso_time) {
                return g_strdup("");
        }

        /* 阶段二：尝试以标准 ISO-8601（含'T'分隔符）进行快速解析 */
        GDateTime *dt = g_date_time_new_from_iso8601(iso_time, NULL);
        if (!dt) {
                /* 
                 * 第一级回退处理：
                 * 针对部分旧版本存储或人工编辑可能采用 "YYYY-MM-DD HH:MM:SS"（空格分隔）的情况，
                 * 将空格动态替换为 'T' 后再次尝试解析。
                 */
                char cleaned[64];
                g_strlcpy(cleaned, iso_time, sizeof(cleaned));
                char *space = strchr(cleaned, ' ');
                if (space) *space = 'T';
                dt = g_date_time_new_from_iso8601(cleaned, NULL);
        }

        /* 若任一解析方式成功获取有效 GDateTime，则使用 GLib 引擎执行模板格式化 */
        if (dt) {
                char *formatted = g_date_time_format(dt, format_str);
                g_date_time_unref(dt);
                if (formatted) return formatted;
        }

        /* 
         * 第二级终极回退机制：
         * 若时间库解析完全失败（例如时区格式不符合规范），但字符串长度足够（至少包含 "YYYY-MM-DDTHH:MM" 16字符），
         * 则根据常见模板需求，直接通过固定偏移量切片提取月-日或时-分，避免界面时间显示缺失。
         */
        if (strlen(iso_time) >= 16) {
                char buf[32];
                if (g_str_has_prefix(format_str, "%m-%d")) {
                        /* 提取 MM-DD HH:MM（偏移 5 取 5 字符为月日，偏移 11 取 5 字符为时分） */
                        g_snprintf(buf, sizeof(buf), "%.5s %.5s", iso_time + 5, iso_time + 11);
                        return g_strdup(buf);
                } else {
                        /* 提取 YYYY-MM-DD HH:MM */
                        g_snprintf(buf, sizeof(buf), "%.10s %.5s", iso_time, iso_time + 11);
                        return g_strdup(buf);
                }
        }

        /* 兜底返回原字符串副本，保证绝对不返回 NULL 引起上层崩溃 */
        return g_strdup(iso_time);
}

/**
 * @brief 将任务状态枚举转换为面向用户的中文展示文案
 * @param status 状态枚举
 * @return const char* 状态中文静态字面量指针
 */
const char *boost_status_to_string(BoostStatus status) {
        switch (status) {
                case BOOST_STATUS_COMPLETED:
                        return "已结算";
                case BOOST_STATUS_TERMINATED:
                        return "已终止";
                case BOOST_STATUS_IN_PROGRESS:
                default:
                        return "进行中";
        }
}

/**
 * @brief 从反序列化得到的整型或字符串多态解析得到标准状态枚举
 * @details 兼容 C# 时代的枚举名称（"InProgress", "Completed", "Terminated"）、
 *          中文标签（"进行中", "已结算", "已终止"）以及序列化数字值（0, 1, 2）。
 * @param status_int 序列化记录的整型状态值
 * @param status_str 序列化记录的字符串状态值（优先使用）
 * @return BoostStatus 解析归一化后的状态枚举值
 */
BoostStatus boost_status_from_int_or_string(int status_int, const char *status_str) {
        /* 优先判定字符串形式的枚举表示 */
        if (status_str && *status_str) {
                /* 匹配已结算状态（兼容英文 Completed 和 中文 已结算） */
                if (g_ascii_strcasecmp(status_str, "Completed") == 0 ||
                        g_strcmp0(status_str, "已结算") == 0) {
                        return BOOST_STATUS_COMPLETED;
                }
                /* 匹配已终止状态（兼容英文 Terminated 和 中文 已终止） */
                if (g_ascii_strcasecmp(status_str, "Terminated") == 0 ||
                        g_strcmp0(status_str, "已终止") == 0) {
                        return BOOST_STATUS_TERMINATED;
                }
                /* 匹配进行中状态（兼容英文 InProgress 和 中文 进行中） */
                if (g_ascii_strcasecmp(status_str, "InProgress") == 0 ||
                        g_strcmp0(status_str, "进行中") == 0) {
                        return BOOST_STATUS_IN_PROGRESS;
                }
        }

        /* 若字符串未匹配或不存在，则依据数值枚举进行第二层映射 */
        if (status_int == 1) return BOOST_STATUS_COMPLETED;
        if (status_int == 2) return BOOST_STATUS_TERMINATED;

        /* 默认为进行中（0） */
        return BOOST_STATUS_IN_PROGRESS;
}

/**
 * @brief 获取与任务状态绑定的视觉主题识别色
 * @param status 任务状态
 * @return const char* 对应的十六进制色彩代码（静态常量）
 */
const char *boost_status_color(BoostStatus status) {
        switch (status) {
                case BOOST_STATUS_COMPLETED:
                        return "#3DDC84"; /* 绿色：代表委托顺利完工、结算到账 */
                case BOOST_STATUS_TERMINATED:
                        return "#FF5C6A"; /* 红色：代表任务取消、中途终止 */
                case BOOST_STATUS_IN_PROGRESS:
                default:
                        return "#8AA0FF"; /* 重点蓝：代表当前正在进行的代肝任务 */
        }
}
