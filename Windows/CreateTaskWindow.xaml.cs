using System;
using System.Windows;
using WaveBoostManager.Models;
using WaveBoostManager.Services;

namespace WaveBoostManager.Windows
{
    public partial class CreateTaskWindow : Window
    {
        private readonly DataService _dataService;

        public CreateTaskWindow(DataService dataService)
        {
            InitializeComponent();
            _dataService = dataService;
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            var title = TxtTitle.Text.Trim();
            if (string.IsNullOrEmpty(title))
            {
                MessageBox.Show("请填写板块标题～", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            var task = new BoostTask
            {
                Title = title,
                Requester = TxtRequester.Text.Trim(),
                Description = TxtDescription.Text.Trim(),
                CreatedAt = DateTime.Now,
                Status = BoostStatus.InProgress
            };

            // 如果填写了详细需求，顺手记一条初始笔录，方便在详情里按时间线查看
            if (!string.IsNullOrWhiteSpace(task.Description))
            {
                task.Notes.Add(new NoteEntry
                {
                    Time = DateTime.Now,
                    Content = "【创建时需求】" + task.Description
                });
            }

            _dataService.AddTask(task);
            DialogResult = true;
        }
    }
}
