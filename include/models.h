/**
 * @file models.h
 * @brief 代肝任务面板 - 核心数据模型与工具函数声明
 * @details 定义了任务状态枚举、跟进笔录结构体、代肝任务结构体以及相关的时间格式化与状态转换工具接口。
 *          为保证内存安全，所有动态分配的字段均遵循所有权归属明确的设计原则。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef MODELS_H
#define MODELS_H

#include <glib.h>

/**
 * @brief 代肝任务的生命周期状态枚举
 * @details 记录任务从创建承接、执行跟进到最终结单或中途取消的完整流转状态。
 */
typedef enum {
        BOOST_STATUS_IN_PROGRESS = 0, /**< 进行中：委托正在执行，位于主界面待办看板中 */
        BOOST_STATUS_COMPLETED = 1,   /**< 已结算：代肝任务已完成且已核对结账，归档至历史记录 */
        BOOST_STATUS_TERMINATED = 2   /**< 已终止：因号主协商或特殊原因提前取消，归档至历史记录 */
} BoostStatus;

/**
 * @brief 任务跟进笔录条目结构体
 * @details 记录代肝过程中的阶段性进展、掉落物、抽卡结果或异常反馈信息。
 */
typedef struct {
        char *time;    /**< 笔录记录时间，格式为标准 ISO-8601 字符串（如 "2026-10-11T04:15:00"），动态分配内存 */
        char *content; /**< 笔录正文内容，记录具体事项或操作日志，动态分配内存 */
} NoteEntry;

/**
 * @brief 代肝委托任务主结构体
 * @details 承载代肝委托的核心业务属性，包括唯一标识、标题、委托人信息、起止时间戳及跟进记录列表。
 */
typedef struct {
        char *id;          /**< 任务全局唯一ID，采用 32 字符十六进制 UUID（无中划线格式，兼容 C# Guid "N" 格式） */
        char *title;       /**< 任务标题（如：“全地图探索度100%”、“肝200抽”等） */
        char *requester;   /**< 委托人 / 号主标识信息（微信名、游戏ID、联系方式备注等） */
        char *description; /**< 需求详细描述，包含委托阶段要求、账号限制或特殊注意细节 */
        char *created_at;  /**< 任务接单创建时间，ISO-8601 字符串（如 "2026-10-11T12:00:00"） */
        char *finished_at; /**< 任务完结/终止时间，未完结时为 NULL，完结时存 ISO-8601 字符串 */
        BoostStatus status;/**< 当前任务状态（进行中 / 已结算 / 已终止） */
        GPtrArray *notes;  /**< 跟进笔录条目数组，内部元素为 NoteEntry*，附带自动释放函数 */
        int rank;          /**< UI 展示排序序号（从 1 开始递增），按接单时间先后动态赋予，非持久化字段 */
} BoostTask;

/* ========================================================================= */
/*                              笔录相关操作接口                             */
/* ========================================================================= */

/**
 * @brief 创建新的笔录条目对象
 * @param content 笔录内容文本，不可为 NULL；若传 NULL 则自动规范为空字符串
 * @param iso_time 指定时间戳（ISO-8601 字符串）；若为 NULL 或空串，则自动获取系统当前时间
 * @return NoteEntry* 新分配的笔录指针，调用方需负责后续的释放或移交托管
 */
NoteEntry *note_entry_new(const char *content, const char *iso_time);

/**
 * @brief 释放笔录条目所占用的堆内存
 * @param note 待释放的笔录指针；若为 NULL 则直接安全返回
 */
void note_entry_free(NoteEntry *note);

/* ========================================================================= */
/*                              任务相关操作接口                             */
/* ========================================================================= */

/**
 * @brief 创建并初始化一个新的代肝任务对象
 * @param title 任务标题（必填项语义）
 * @param requester 委托人信息（可为空，内部做安全空值兜底）
 * @param description 需求详情说明（可为空，内部做安全空值兜底）
 * @return BoostTask* 新初始化的任务实例指针，默认状态为进行中，分配独立 UUID
 */
BoostTask *boost_task_new(const char *title, const char *requester, const char *description);

/**
 * @brief 深度释放代肝任务对象及其内部所有字段和笔录列表
 * @param task 待释放的任务指针；若为 NULL 则直接安全返回
 */
void boost_task_free(BoostTask *task);

/**
 * @brief 为指定任务追加一条跟进笔录
 * @param task 目标任务指针
 * @param content 笔录文本内容，追加时自动获取当前系统时间作为时间戳
 */
void boost_task_add_note(BoostTask *task, const char *content);

/* ========================================================================= */
/*                              时间处理工具接口                             */
/* ========================================================================= */

/**
 * @brief 获取当前系统本地时间的 ISO-8601 格式字符串
 * @return char* 格式如 "YYYY-MM-DDTHH:MM:SS" 的动态分配字符串，使用完毕后需 g_free()
 */
char *get_current_iso_time(void);

/**
 * @brief 将 ISO-8601 字符串解析并转换为用户友好的界面展示文本
 * @param iso_time 原始 ISO-8601 格式时间文本
 * @param format_str 目标格式化模板（如 "%Y-%m-%d %H:%M" 或 "%m-%d %H:%M"）
 * @return char* 格式化后的时间文本，动态分配内存，使用完毕后需 g_free()
 */
char *format_display_time(const char *iso_time, const char *format_str);

/* ========================================================================= */
/*                              状态处理工具接口                             */
/* ========================================================================= */

/**
 * @brief 将任务状态枚举转换为对应的中文显示标签
 * @param status 任务状态枚举
 * @return const char* 状态中文名称常量字符串（静态区常量，无需释放）
 */
const char *boost_status_to_string(BoostStatus status);

/**
 * @brief 兼容整数数值或字符串名称转换为标准的任务状态枚举
 * @details 用于反序列化旧版/C#版本存储的数值（0,1,2）或中英文状态文本
 * @param status_int 状态整型值
 * @param status_str 状态字符串（例如 "InProgress", "Completed", "已结算" 等）
 * @return BoostStatus 解析得到的标准状态枚举
 */
BoostStatus boost_status_from_int_or_string(int status_int, const char *status_str);

/**
 * @brief 获取与任务状态对应的界面高亮主题色（十六进制颜色值）
 * @param status 任务状态枚举
 * @return const char* 十六进制颜色代码字符串（如 "#3DDC84"）
 */
const char *boost_status_color(BoostStatus status);

#endif /* MODELS_H */
