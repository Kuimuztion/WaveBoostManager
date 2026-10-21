/**
 * @file data_service.c
 * @brief 代肝任务面板 - 数据持久化服务层实现
 * @details 实现了基于 cJSON 引擎的 tasks.json 数据文件加载、序列化保存、增量修改，
 *          以及按业务状态过滤和排序的逻辑。兼容原 C# 版生成的 JSON 格式与字段命名。
 *
 * @copyright Copyright (c) 2026 LinusTrevian. Licensed under the MIT License.
 */

#include "data_service.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 解析并获取任务数据文件的标准存储路径
 * @details 按照 Linux 桌面标准规范，将数据放置于用户主目录下的 `.config/WaveBoostManager/tasks.json`。
 *          函数执行过程中会自动检测并递归创建该目录，防止首次运行因目录不存在导致写入失败。
 * @return char* 拼接后的完整绝对路径，由调用方负责 g_free()
 */
char *data_service_get_storage_path(void) {
        /*
         * 0. 优先检测测试与沙箱环境变量覆盖：
         * 若外部或测试套件设置了 WAVE_BOOST_STORAGE_PATH 环境变量，
         * 则直接返回该自定义路径，彻底将单元测试与真实用户的生产配置目录完全物理隔离，
         * 杜绝自动化测试意外修改或删除真实用户的代肝数据。
         */
        const char *env_override = g_getenv("WAVE_BOOST_STORAGE_PATH");
        if (env_override && *env_override) {
                return g_strdup(env_override);
        }

        /* 
         * 1. 获取 Linux XDG 用户配置目录根路径：
         * 通常为 ~/.config，若环境变量未设置则 GLib 会自动兜底为 ~/.config
         */
        const char *config_dir = g_get_user_config_dir();

        /* 
         * 2. 组装 WaveBoostManager 应用专属配置目录路径，
         * 并递归创建目录（若已存在则忽略），赋予当前用户 0755 标准读写执行权限
         */
        char *dir = g_build_filename(config_dir, "WaveBoostManager", NULL);
        g_mkdir_with_parents(dir, 0755);

        /* 3. 在配置目录下拼接目标数据文件名 tasks.json */
        char *file_path = g_build_filename(dir, "tasks.json", NULL);

        /* 释放中间目录临时字符串 */
        g_free(dir);

        return file_path;
}

/**
 * @brief 从 cJSON 对象中安全提取字符串字段值（大小写键名双向兼容）
 * @details C# 序列化器默认采用 PascalCase（如 "Title"、"CreatedAt"），
 *          而通用 Web/JSON 规范多倾向于 camelCase（如 "title"、"createdAt"）。
 *          本函数依次检索两种候选键名，增强数据解析的兼容性与容错能力。
 * @param obj 待查询的 cJSON 对象指针
 * @param key1 首选键名（例如大写开头的 "Title"）
 * @param key2 备选键名（例如小写开头的 "title"）
 * @return const char* 字段内部字符串指针（生命周期受限于 obj），若不存在或非字符串类型则返回 NULL
 */
static const char *get_json_string(const cJSON *obj, const char *key1, const char *key2) {
        /* 尝试使用区分大小写的快速匹配查找首选键名 */
        cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key1);

        /* 若未命中且提供了备选键名，则尝试检索备选键名 */
        if (!item && key2) {
                item = cJSON_GetObjectItemCaseSensitive(obj, key2);
        }

        /* 严格校验目标节点类型必须为有效字符串且内容指针非空 */
        if (item && cJSON_IsString(item) && item->valuestring) {
                return item->valuestring;
        }

        return NULL;
}

/**
 * @brief 将单个 cJSON 任务对象反序列化解析为内存中的 BoostTask 结构体
 * @details 负责解析基本信息字段、空值防护兜底、状态枚举转换以及子项跟进笔录列表的循环构建。
 * @param obj 单个任务的 cJSON 节点
 * @return BoostTask* 解析成功返回新分配的任务指针；若传入节点不是有效 JSON 对象则返回 NULL
 */
static BoostTask *parse_task_json(const cJSON *obj) {
        /* 卫语句：验证传入节点必须是合法的 JSON 对象类型 */
        if (!cJSON_IsObject(obj)) return NULL;

        /* 
         * 1. 提取所有基础文本字段：
         * 分别适配大驼峰与小驼峰两种键名风格，防止因配置文件来源不同引发字段丢失。
         */
        const char *id_str = get_json_string(obj, "Id", "id");
        const char *title_str = get_json_string(obj, "Title", "title");
        const char *req_str = get_json_string(obj, "Requester", "requester");
        const char *desc_str = get_json_string(obj, "Description", "description");
        const char *created_str = get_json_string(obj, "CreatedAt", "createdAt");
        const char *finished_str = get_json_string(obj, "FinishedAt", "finishedAt");

        /* 分配并零初始化任务结构体 */
        BoostTask *task = g_new0(BoostTask, 1);

        /* 
         * 2. 任务唯一 ID 容错保护：
         * 若读取到的 ID 为空串或不存在，则自动生成新的随机 UUID，保证内存中所有任务实例 ID 均具备唯一性。
         */
        task->id = g_strdup(id_str ? id_str : "");
        if (!*task->id) {
                g_free(task->id);
                char *uuid = g_uuid_string_random();
                task->id = uuid;
        }

        /* 字符串字段安全复制 */
        task->title = g_strdup(title_str ? title_str : "");
        task->requester = g_strdup(req_str ? req_str : "");
        task->description = g_strdup(desc_str ? desc_str : "");

        /* 
         * 3. 创建时间容错保护：
         * 若历史数据缺失创建时间，自动以当前系统时间兜底，防止时间排序计算时引发异常。
         */
        task->created_at = g_strdup(created_str ? created_str : "");
        if (!*task->created_at) {
                g_free(task->created_at);
                task->created_at = get_current_iso_time();
        }

        /* 完成时间：仅当存在有效非空字符串时分配，否则保持 NULL 表示尚未结束 */
        task->finished_at = (finished_str && *finished_str) ? g_strdup(finished_str) : NULL;

        /* 
         * 4. 解析任务状态（Status）：
         * 检查 JSON 中可能存在的数值型或字符串型状态字段，通过通用转换器映射为标准枚举。
         */
        cJSON *status_item = cJSON_GetObjectItemCaseSensitive(obj, "Status");
        if (!status_item) status_item = cJSON_GetObjectItemCaseSensitive(obj, "status");
        int status_int = 0;
        const char *status_str = NULL;
        if (status_item) {
                if (cJSON_IsNumber(status_item)) {
                        status_int = status_item->valueint;
                } else if (cJSON_IsString(status_item)) {
                        status_str = status_item->valuestring;
                }
        }
        task->status = boost_status_from_int_or_string(status_int, status_str);

        /* 
         * 5. 解析跟进笔录列表（Notes 数组）：
         * 初始化带有析构回调的 GPtrArray 容器，遍历提取子项笔录的时间与正文。
         */
        task->notes = g_ptr_array_new_with_free_func((GDestroyNotify)note_entry_free);
        cJSON *notes_arr = cJSON_GetObjectItemCaseSensitive(obj, "Notes");
        if (!notes_arr) notes_arr = cJSON_GetObjectItemCaseSensitive(obj, "notes");
        if (cJSON_IsArray(notes_arr)) {
                cJSON *note_item = NULL;
                cJSON_ArrayForEach(note_item, notes_arr) {
                        if (!cJSON_IsObject(note_item)) continue;
                        const char *ntime = get_json_string(note_item, "Time", "time");
                        const char *ncont = get_json_string(note_item, "Content", "content");
                        /* 只要笔录正文存在有效指针即可入库 */
                        if (ncont) {
                                NoteEntry *ne = note_entry_new(ncont, ntime);
                                g_ptr_array_add(task->notes, ne);
                        }
                }
        }

        return task;
}

/**
 * @brief 从磁盘读取并加载所有任务记录
 * @details 采用双重路径检索逻辑（标准配置目录优先，当前运行目录兜底），
 *          并具备完整的 cJSON 语法树解析与空文件/损坏文件的安全容错机制。
 * @return GPtrArray* 包含所有有效 BoostTask 实例的指针动态数组
 */
GPtrArray *data_service_load_all(void) {
        char *path = data_service_get_storage_path();
        /* 创建容纳任务实体的动态数组，注册释放回调 boost_task_free */
        GPtrArray *list = g_ptr_array_new_with_free_func((GDestroyNotify)boost_task_free);

        char *contents = NULL;
        gsize length = 0;

        /* 
         * 步骤一：优先从标准配置路径读取文件内容。
         * 若读取失败（如首次运行或迁移未同步），则尝试从当前工作目录的 tasks.json 回退读取。
         */
        if (!g_file_get_contents(path, &contents, &length, NULL)) {
                if (!g_file_get_contents("tasks.json", &contents, &length, NULL)) {
                        /* 两个路径均无可用文件，说明为全新环境，直接返回空列表 */
                        g_free(path);
                        return list;
                }
        }
        g_free(path);

        /* 步骤二：若文件内容为空或长度为 0，释放缓冲区并直接返回空列表 */
        if (!contents || length == 0) {
                g_free(contents);
                return list;
        }

        /* 步骤三：利用 cJSON 高效解析指定长度的 JSON 文本字符串 */
        cJSON *root = cJSON_ParseWithLength(contents, length);
        g_free(contents); /* 解析完毕后即刻释放原始文件字符串，节省内存 */

        /* 语法错误防护：若解析失败则返回空列表 */
        if (!root) {
                return list;
        }

        /* 
         * 步骤四：校验根节点必须为数组类型，并逐项反序列化注入列表。
         * tasks.json 的根结构即为一个任务对象的数组 [ {...}, {...} ]。
         */
        if (cJSON_IsArray(root)) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, root) {
                        BoostTask *task = parse_task_json(item);
                        if (task) {
                                g_ptr_array_add(list, task);
                        }
                }
        }

        /* 释放 cJSON 语法树占用的内存结构 */
        cJSON_Delete(root);
        return list;
}

/**
 * @brief 将内存中的任务列表完整持久化到 JSON 文件
 * @details 重新构筑 cJSON 数组，序列化为具备良好排版缩进的文本，
 *          并通过 GLib 的原子化文件写入接口落地磁盘，杜绝意外断电或崩溃损坏文件。
 * @param tasks 待持久化的任务指针动态数组
 * @return gboolean 写入成功返回 TRUE，失败返回 FALSE
 */
gboolean data_service_save_all(GPtrArray *tasks) {
        /* 创建根 JSON 数组对象 */
        cJSON *root = cJSON_CreateArray();
        if (!root) return FALSE;

        /* 
         * 遍历内存中的所有任务并将其属性映射至 JSON 键值对：
         * 保持采用与原系统一致的 PascalCase 键名风格，方便跨平台交互。
         */
        if (tasks) {
                for (guint i = 0; i < tasks->len; i++) {
                        BoostTask *task = g_ptr_array_index(tasks, i);
                        cJSON *obj = cJSON_CreateObject();

                        cJSON_AddStringToObject(obj, "Id", task->id ? task->id : "");
                        cJSON_AddStringToObject(obj, "Title", task->title ? task->title : "");
                        cJSON_AddStringToObject(obj, "Requester", task->requester ? task->requester : "");
                        cJSON_AddStringToObject(obj, "Description", task->description ? task->description : "");
                        cJSON_AddStringToObject(obj, "CreatedAt", task->created_at ? task->created_at : "");

                        /* 完结时间：有值存字符串，无值存标准 JSON null */
                        if (task->finished_at && *task->finished_at) {
                                cJSON_AddStringToObject(obj, "FinishedAt", task->finished_at);
                        } else {
                                cJSON_AddNullToObject(obj, "FinishedAt");
                        }

                        /* 状态存数字值，兼容各种解析端 */
                        cJSON_AddNumberToObject(obj, "Status", (double)task->status);

                        /* 构造笔录条目的子数组 */
                        cJSON *notes_arr = cJSON_CreateArray();
                        if (task->notes) {
                                for (guint j = 0; j < task->notes->len; j++) {
                                        NoteEntry *note = g_ptr_array_index(task->notes, j);
                                        cJSON *nobj = cJSON_CreateObject();
                                        cJSON_AddStringToObject(nobj, "Time", note->time ? note->time : "");
                                        cJSON_AddStringToObject(nobj, "Content", note->content ? note->content : "");
                                        cJSON_AddItemToArray(notes_arr, nobj);
                                }
                        }
                        cJSON_AddItemToObject(obj, "Notes", notes_arr);

                        /* 将任务对象挂载到根数组中 */
                        cJSON_AddItemToArray(root, obj);
                }
        }

        /* 格式化为美化缩进的 JSON 文本 */
        char *json_text = cJSON_Print(root);
        cJSON_Delete(root);

        if (!json_text) return FALSE;

        /* 
         * 写入磁盘文件：
         * g_file_set_contents 内部采用安全写入策略（写入临时文件后通过 rename 原子替换），
         * 能有效防止写入中途崩溃导致文件内容变为空白或截断。
         */
        char *path = data_service_get_storage_path();
        GError *err = NULL;
        gboolean ok = g_file_set_contents(path, json_text, -1, &err);
        if (!ok && err) {
                g_warning("Failed to save tasks to %s: %s", path, err->message);
                g_error_free(err);
        }

        /* 清理路径与 JSON 文本缓存 */
        g_free(path);
        cJSON_free(json_text);
        return ok;
}

/**
 * @brief 新增代肝任务到持久化存储
 * @details 采用先加载最新数据、再插入深拷贝副本、最后全量持久化的防脏读流程。
 * @param task 待插入的任务对象指针
 */
void data_service_add_task(BoostTask *task) {
        /* 卫语句拦截非法空指针 */
        if (!task) return;

        /* 读取当前持久化中的全量任务列表 */
        GPtrArray *all = data_service_load_all();

        /* 
         * 构建任务深拷贝副本：
         * 避免调用端后续对入参指针的改动与存储层的已持久化状态发生交叉干扰。
         */
        BoostTask *copy = boost_task_new(task->title, task->requester, task->description);
        g_free(copy->id);
        copy->id = g_strdup(task->id);
        g_free(copy->created_at);
        copy->created_at = g_strdup(task->created_at);
        copy->finished_at = task->finished_at ? g_strdup(task->finished_at) : NULL;
        copy->status = task->status;

        /* 深度复制所有的子项笔录 */
        if (task->notes) {
                for (guint i = 0; i < task->notes->len; i++) {
                        NoteEntry *ne = g_ptr_array_index(task->notes, i);
                        boost_task_add_note(copy, ne->content);
                }
        }

        /* 追加到列表末尾并落盘 */
        g_ptr_array_add(all, copy);
        data_service_save_all(all);

        /* 释放操作列表内存 */
        g_ptr_array_unref(all);
}

/**
 * @brief 根据任务 ID 更新现有代肝任务或作为新记录插入
 * @details 检索列表，若 ID 相同则用新的深拷贝替换原有对象；若不存在则末尾追加。
 * @param updated 包含最新修改的任务对象
 */
void data_service_update_task(BoostTask *updated) {
        /* 卫语句：校验任务对象及唯一 ID 必须有效 */
        if (!updated || !updated->id) return;

        GPtrArray *all = data_service_load_all();
        int found_idx = -1;

        /* 遍历全量任务查找目标 ID */
        for (guint i = 0; i < all->len; i++) {
                BoostTask *t = g_ptr_array_index(all, i);
                if (g_strcmp0(t->id, updated->id) == 0) {
                        found_idx = (int)i;
                        break;
                }
        }

        /* 
         * 构建包含最新字段内容的完整任务副本：
         * 包括笔录列表、起止时间与状态枚举。
         */
        BoostTask *copy = boost_task_new(updated->title, updated->requester, updated->description);
        g_free(copy->id);
        copy->id = g_strdup(updated->id);
        g_free(copy->created_at);
        copy->created_at = g_strdup(updated->created_at);
        copy->finished_at = updated->finished_at ? g_strdup(updated->finished_at) : NULL;
        copy->status = updated->status;

        /* 深度拷贝笔录（包括原本记录的精确历史时间戳） */
        if (updated->notes) {
                for (guint i = 0; i < updated->notes->len; i++) {
                        NoteEntry *ne = g_ptr_array_index(updated->notes, i);
                        NoteEntry *ne_copy = note_entry_new(ne->content, ne->time);
                        g_ptr_array_add(copy->notes, ne_copy);
                }
        }

        if (found_idx >= 0) {
                /* 
                 * 替换旧任务实体：
                 * 显式释放原索引处的旧结构体内存，随后将新副本指针赋予该位置
                 */
                BoostTask *old = g_ptr_array_index(all, found_idx);
                boost_task_free(old);
                all->pdata[found_idx] = copy;
        } else {
                /* 未找到对应 ID，则作为新记录追加至末尾 */
                g_ptr_array_add(all, copy);
        }

        /* 写入存储并释放全量列表包装 */
        data_service_save_all(all);
        g_ptr_array_unref(all);
}

/**
 * @brief 进行中任务排序比较函数（按接单创建时间正序排列）
 * @details ISO-8601 字符串（YYYY-MM-DDTHH:MM:SS）具有字典序与时间序严格一致的优秀特性，
 *          因此直接使用 g_strcmp0 即可高精、高效地完成按时间先后的升序排序。
 * @param a 元素指针 A
 * @param b 元素指针 B
 * @return gint 比较结果：<0 表示 a 早于 b，0 表示相同，>0 表示 a 晚于 b
 */
static gint compare_in_progress(gconstpointer a, gconstpointer b) {
        const BoostTask *ta = *(const BoostTask **)a;
        const BoostTask *tb = *(const BoostTask **)b;
        const char *ca = ta->created_at ? ta->created_at : "";
        const char *cb = tb->created_at ? tb->created_at : "";
        return g_strcmp0(ca, cb);
}

/**
 * @brief 获取所有进行中的任务，并根据接单先后赋予 rank 序号
 * @details 采用所有权转移技术（剥离原列表所有权），避免频繁深拷贝的性能开销。
 * @return GPtrArray* 进行中任务列表（已排序并已计算 rank）
 */
GPtrArray *data_service_get_in_progress(void) {
        /* 1. 加载所有任务 */
        GPtrArray *all = data_service_load_all();
        /* 2. 创建结果过滤数组 */
        GPtrArray *result = g_ptr_array_new_with_free_func((GDestroyNotify)boost_task_free);

        /* 
         * 3. 筛选进行中任务：
         * 为提升性能，将命中项的所有权从原数组直接转移至 result。
         * 将原数组的槽位重置为 NULL，这样后续 g_ptr_array_unref(all) 时就不会销毁已转移的任务对象。
         */
        for (guint i = 0; i < all->len; i++) {
                BoostTask *t = g_ptr_array_index(all, i);
                if (t->status == BOOST_STATUS_IN_PROGRESS) {
                        all->pdata[i] = NULL; /* 剥离所有权 */
                        g_ptr_array_add(result, t);
                }
        }
        g_ptr_array_unref(all);

        /* 4. 按接单创建时间升序排序（排在最上面的为最早接单的代肝委托） */
        g_ptr_array_sort(result, compare_in_progress);

        /* 
         * 5. 动态计算序号 Rank：
         * 从 1 开始编号（NO.1, NO.2 ...），方便号主与代肝直观查看排单先后。
         */
        for (guint i = 0; i < result->len; i++) {
                BoostTask *t = g_ptr_array_index(result, i);
                t->rank = (int)i + 1;
        }

        return result;
}

/**
 * @brief 历史记录排序比较函数（按结单时间/创建时间降序排列）
 * @details 越近完成或终止的任务排在最前面，方便用户快速查看最近结单或处理过的委托。
 * @param a 元素指针 A
 * @param b 元素指针 B
 * @return gint 降序比较结果
 */
static gint compare_history(gconstpointer a, gconstpointer b) {
        const BoostTask *ta = *(const BoostTask **)a;
        const BoostTask *tb = *(const BoostTask **)b;
        /* 优先取 FinishedAt 时间，若无 FinishedAt 则回退取 CreatedAt */
        const char *ta_time = ta->finished_at ? ta->finished_at : (ta->created_at ? ta->created_at : "");
        const char *tb_time = tb->finished_at ? tb->finished_at : (tb->created_at ? tb->created_at : "");
        /* 降序排列：用 tb 比较 ta */
        return g_strcmp0(tb_time, ta_time);
}

/**
 * @brief 获取所有已归档（已结算或已终止）的历史代肝任务
 * @return GPtrArray* 已排序的历史任务列表
 */
GPtrArray *data_service_get_history(void) {
        GPtrArray *all = data_service_load_all();
        GPtrArray *result = g_ptr_array_new_with_free_func((GDestroyNotify)boost_task_free);

        /* 
         * 筛选非进行中任务并转移所有权：
         * status != BOOST_STATUS_IN_PROGRESS 涵盖了已结算和已终止两类状态。
         */
        for (guint i = 0; i < all->len; i++) {
                BoostTask *t = g_ptr_array_index(all, i);
                if (t->status != BOOST_STATUS_IN_PROGRESS) {
                        all->pdata[i] = NULL; /* 剥离所有权，转移至结果列表 */
                        g_ptr_array_add(result, t);
                }
        }
        g_ptr_array_unref(all);

        /* 按照完结时间倒序排列（最新的排在最上） */
        g_ptr_array_sort(result, compare_history);
        return result;
}

/**
 * @brief 按照任务唯一 ID 查询任务对象
 * @param id 任务 32 位 UUID 字符串
 * @return BoostTask* 查询到的任务独立对象指针，若不存在则为 NULL
 */
BoostTask *data_service_find_by_id(const char *id) {
        /* 防御性参数校验 */
        if (!id || !*id) return NULL;

        GPtrArray *all = data_service_load_all();
        BoostTask *found = NULL;

        /* 线性扫描全量任务 */
        for (guint i = 0; i < all->len; i++) {
                BoostTask *t = g_ptr_array_index(all, i);
                if (g_strcmp0(t->id, id) == 0) {
                        /* 剥离所有权转交给调用方，并清空原数组该项防止被自动释放 */
                        all->pdata[i] = NULL;
                        found = t;
                        break;
                }
        }

        /* 释放容器本身及其他未命中的任务内存 */
        g_ptr_array_unref(all);
        return found;
}
