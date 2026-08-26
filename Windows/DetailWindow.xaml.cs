using System;
using System.Linq;
using System.Windows;
using WaveBoostManager.Models;
using WaveBoostManager.Services;

namespace WaveBoostManager.Windows
{
    /// <summary>
    /// 单条笔录在界面上的展示包装
    /// </summary>
    public class NoteDisplayItem
    {
        public string TimeText { get; set; } = string.Empty;
        public string Content { get; set; } = string.Empty;
    }

    public partial class DetailWindow : Window
    {
        private readonly DataService _dataService;
        private readonly string _taskId;
        private readonly bool _readOnly;
        private BoostTask _task = null!;

        public DetailWindow(DataService dataService, string taskId, bool readOnly)
        {
            InitializeComponent();
            _dataService = dataService;
            _taskId = taskId;
            _readOnly = readOnly;
            LoadTask();
        }

        private void LoadTask()
        {
            var all = _dataService.LoadAll();
            var found = all.FirstOrDefault(t => t.Id == _taskId);
            if (found == null)
            {
                MessageBox.Show("未找到该板块，可能已被删除。", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
                Close();
                return;
            }

            _task = found;
            RenderTask();
        }

        private void RenderTask()
        {
            TxtTitle.Text = _task.Title;
            TxtStatus.Text = _task.StatusText;
            TxtStatus.Foreground = (System.Windows.Media.Brush)new System.Windows.Media.BrushConverter()
                .ConvertFromString(_task.StatusColor)!;

            var who = string.IsNullOrWhiteSpace(_task.Requester) ? "未填写" : _task.Requester;
            var timeInfo = $"创建于 {_task.CreatedAt:yyyy-MM-dd HH:mm}";
            if (_task.FinishedAt.HasValue)
            {
                var verb = _task.Status == BoostStatus.Completed ? "结算" : "终止";
                timeInfo += $"　·　{verb}于 {_task.FinishedAt.Value:yyyy-MM-dd HH:mm}";
            }
            TxtMeta.Text = $"委托人：{who}　·　{timeInfo}";

            TxtDescription.Text = _task.Description;

            NotesList.ItemsSource = _task.Notes
                .OrderByDescending(n => n.Time)
                .Select(n => new NoteDisplayItem
                {
                    TimeText = n.Time.ToString("yyyy-MM-dd HH:mm"),
                    Content = n.Content
                })
                .ToList();

            // 已结算或已终止的板块不再允许编辑，仅供查看
            if (_readOnly || _task.Status != BoostStatus.InProgress)
            {
                TxtDescription.IsReadOnly = true;
                AddNotePanel.Visibility = Visibility.Collapsed;
                BtnSave.Visibility = Visibility.Collapsed;
            }
        }

        private void BtnAddNote_Click(object sender, RoutedEventArgs e)
        {
            var content = TxtNewNote.Text.Trim();
            if (string.IsNullOrEmpty(content)) return;

            _task.Notes.Add(new NoteEntry
            {
                Time = DateTime.Now,
                Content = content
            });
            _dataService.UpdateTask(_task);

            TxtNewNote.Text = string.Empty;
            RenderTask();
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            _task.Description = TxtDescription.Text.Trim();
            _dataService.UpdateTask(_task);
            MessageBox.Show("已保存～", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
