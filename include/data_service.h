/**
 * @file data_service.h
 * @brief 代肝任务面板 - 数据持久化服务层接口
 * @details 负责任务与笔录数据的 JSON 文件存取、增删改查（CRUD）、状态筛选与业务排序。
 *          默认持久化存储路径为标准 XDG 配置目录：~/.config/WaveBoostManager/tasks.json。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#include "models.h"
#include <glib.h>

/**
 * @brief 获取任务数据持久化文件（tasks.json）的绝对物理存储路径
 * @details 遵循 XDG 基础目录规范，通常解析为 `$XDG_CONFIG_HOME/WaveBoostManager/tasks.json`
 *          （即 `~/.config/WaveBoostManager/tasks.json`）。该接口会自动确保父级目录的存在。
 * @return char* 动态分配的文件绝对路径字符串，调用方使用完后须调用 g_free() 释放
 */
char *data_service_get_storage_path(void);

/**
 * @brief 从持久化存储文件中加载全部任务记录
 * @details 读取 tasks.json 内容并利用 cJSON 进行反序列化构建任务对象列表。
 *          若配置文件不存在，会自动回退尝试读取运行目录下的旧版 tasks.json；
 *          若均不存在或文件为空，则返回一个初始化完成的空列表（元素类型为 BoostTask*）。
 * @return GPtrArray* 任务对象指针数组，内含自动释放析构函数 (boost_task_free)，
 *                    调用方在不需要该数组时需调用 g_ptr_array_unref() 进行整体释放
 */
GPtrArray *data_service_load_all(void);

/**
 * @brief 将完整的任务列表序列化并持久化写入存储文件
 * @details 将所有 BoostTask 及其子项 NoteEntry 构筑为 cJSON 语法树，
 *          随后使用带有原子性替换机制的 g_file_set_contents 写入磁盘，防止程序异常中断导致数据损坏。
 * @param tasks 待持久化的任务指针动态数组（允许为 NULL 或空数组，写入 "[]"）
 * @return gboolean 写入操作成功返回 TRUE，失败返回 FALSE 并输出警告日志
 */
gboolean data_service_save_all(GPtrArray *tasks);

/**
 * @brief 新增一条代肝委托任务并立即持久化入库
 * @details 对传入的任务对象执行深拷贝后追加至当前任务列表并写入磁盘，
 *          不会夺取传入指针的所有权，调用方依然持有并需自行管理原指针生命周期。
 * @param task 待新增的任务对象指针（若为 NULL 则直接忽略）
 */
void data_service_add_task(BoostTask *task);

/**
 * @brief 更新现有任务的信息（依据任务全局唯一 ID 匹配）
 * @details 加载全部数据，查找与 updated->id 相同的条目并用传入对象的深拷贝进行替换；
 *          若未找到相同 ID 的旧记录，则作为新任务追加至末尾。更新完成后立即保存至磁盘。
 * @param updated 包含最新字段状态的任务对象指针
 */
void data_service_update_task(BoostTask *updated);

/**
 * @brief 提取当前处于“进行中”状态的所有代肝任务
 * @details 筛选出 status == BOOST_STATUS_IN_PROGRESS 的任务，
 *          按创建接单时间（CreatedAt）进行正序排序（越早接的排在前面），
 *          并为每一项动态计算赋予 rank 序号（1, 2, 3...）。
 * @return GPtrArray* 筛选并排序后的任务指针数组，调用方负责 g_ptr_array_unref()
 */
GPtrArray *data_service_get_in_progress(void);

/**
 * @brief 提取所有已归档的历史任务（已结算或已终止）
 * @details 筛选出 status != BOOST_STATUS_IN_PROGRESS 的记录，
 *          按照结单/终止时间（若无则按创建时间）进行降序排序（越近完结的排在越前面）。
 * @return GPtrArray* 历史任务指针数组，调用方负责 g_ptr_array_unref()
 */
GPtrArray *data_service_get_history(void);

/**
 * @brief 依据任务唯一 ID 查询指定任务的完整最新数据
 * @param id 任务的 32 字符十六进制 UUID
 * @return BoostTask* 查询命中的任务对象独立实例指针；若未找到或 ID 无效则返回 NULL。
 *                    返回的对象所有权归调用方所有，使用完须调用 boost_task_free() 释放
 */
BoostTask *data_service_find_by_id(const char *id);

#endif /* DATA_SERVICE_H */
