using System;
using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace WaveBoostManager.Models
{
    /// <summary>
    /// 任务状态：进行中 / 已结算完成 / 已终止销毁
    /// </summary>
    public enum BoostStatus
    {
        InProgress = 0,
        Completed = 1,
        Terminated = 2
    }

    /// <summary>
    /// 一条笔录（跟进记录），带时间戳
    /// </summary>
    public class NoteEntry
    {
        public DateTime Time { get; set; } = DateTime.Now;
        public string Content { get; set; } = string.Empty;
    }

    /// <summary>
    /// 一个"代肝板块"（任务卡片）
    /// </summary>
    public class BoostTask
    {
        public string Id { get; set; } = Guid.NewGuid().ToString("N");

        /// <summary>板块标题，例如 "肝200抽"</summary>
        public string Title { get; set; } = string.Empty;

        /// <summary>委托人 / 号主信息（备注名、联系方式、账号等）</summary>
        public string Requester { get; set; } = string.Empty;

        /// <summary>任务的详细描述 / 具体需求</summary>
        public string Description { get; set; } = string.Empty;

        public DateTime CreatedAt { get; set; } = DateTime.Now;

        public DateTime? FinishedAt { get; set; }

        public BoostStatus Status { get; set; } = BoostStatus.InProgress;

        public List<NoteEntry> Notes { get; set; } = new();

        // ---------- 以下为界面展示用的辅助属性，不写入 JSON ----------

        [JsonIgnore]
        public int Rank { get; set; }

        [JsonIgnore]
        public string RankDisplay => $"NO.{Rank}";

        [JsonIgnore]
        public string SubInfo
        {
            get
            {
                var who = string.IsNullOrWhiteSpace(Requester) ? "未填写委托人" : $"委托人：{Requester}";
                return $"{who}    创建于 {CreatedAt:MM-dd HH:mm}    笔录 {Notes.Count} 条";
            }
        }

        [JsonIgnore]
        public string StatusText => Status switch
        {
            BoostStatus.Completed => "已结算",
            BoostStatus.Terminated => "已终止",
            _ => "进行中"
        };

        [JsonIgnore]
        public string StatusColor => Status switch
        {
            BoostStatus.Completed => "#3DDC84",
            BoostStatus.Terminated => "#FF5C6A",
            _ => "#8AA0FF"
        };

        [JsonIgnore]
        public string HistorySubInfo
        {
            get
            {
                var timeInfo = FinishedAt.HasValue
                    ? $"{(Status == BoostStatus.Completed ? "结算" : "终止")}于 {FinishedAt.Value:MM-dd HH:mm}"
                    : "";
                return $"创建于 {CreatedAt:MM-dd HH:mm}    {timeInfo}    笔录 {Notes.Count} 条";
            }
        }
    }
}
