using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using WaveBoostManager.Models;

namespace WaveBoostManager.Services
{
    /// <summary>
    /// 负责把任务数据保存到 %AppData%\WaveBoostManager\tasks.json
    /// 每次读写都是全量的，逻辑简单，适合个人单机使用。
    /// </summary>
    public class DataService
    {
        private readonly string _filePath;
        private static readonly JsonSerializerOptions JsonOptions = new()
        {
            WriteIndented = true
        };

        public DataService()
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "WaveBoostManager");
            Directory.CreateDirectory(dir);
            _filePath = Path.Combine(dir, "tasks.json");
        }

        public List<BoostTask> LoadAll()
        {
            try
            {
                if (!File.Exists(_filePath))
                    return new List<BoostTask>();

                var json = File.ReadAllText(_filePath);
                if (string.IsNullOrWhiteSpace(json))
                    return new List<BoostTask>();

                return JsonSerializer.Deserialize<List<BoostTask>>(json) ?? new List<BoostTask>();
            }
            catch
            {
                // 数据文件损坏时不让程序崩溃，返回空列表
                return new List<BoostTask>();
            }
        }

        public void SaveAll(List<BoostTask> tasks)
        {
            var json = JsonSerializer.Serialize(tasks, JsonOptions);
            File.WriteAllText(_filePath, json);
        }

        public void AddTask(BoostTask task)
        {
            var all = LoadAll();
            all.Add(task);
            SaveAll(all);
        }

        public void UpdateTask(BoostTask updated)
        {
            var all = LoadAll();
            var idx = all.FindIndex(t => t.Id == updated.Id);
            if (idx >= 0)
                all[idx] = updated;
            else
                all.Add(updated);
            SaveAll(all);
        }

        public List<BoostTask> GetInProgress()
        {
            return LoadAll()
                .Where(t => t.Status == BoostStatus.InProgress)
                .OrderBy(t => t.CreatedAt)
                .ToList();
        }

        public List<BoostTask> GetHistory()
        {
            return LoadAll()
                .Where(t => t.Status != BoostStatus.InProgress)
                .OrderByDescending(t => t.FinishedAt ?? t.CreatedAt)
                .ToList();
        }
    }
}
