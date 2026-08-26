using System.Windows;
using WaveBoostManager.Models;
using WaveBoostManager.Services;
using WaveBoostManager.Windows;

namespace WaveBoostManager
{
    public partial class MainWindow : Window
    {
        private readonly DataService _dataService = new();

        public MainWindow()
        {
            InitializeComponent();
            Refresh();
        }

        private void Refresh()
        {
            var tasks = _dataService.GetInProgress();
            for (int i = 0; i < tasks.Count; i++)
            {
                tasks[i].Rank = i + 1;
            }

            TaskList.ItemsSource = tasks;
            TxtEmptyHint.Visibility = tasks.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        private void BtnTheme_Click(object sender, RoutedEventArgs e)
        {
            ((App)Application.Current).ToggleTheme();
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            var win = new CreateTaskWindow(_dataService) { Owner = this };
            if (win.ShowDialog() == true)
            {
                Refresh();
            }
        }

        private void BtnHistory_Click(object sender, RoutedEventArgs e)
        {
            var win = new HistoryWindow(_dataService) { Owner = this };
            win.ShowDialog();
            Refresh();
        }

        private void View_Click(object sender, RoutedEventArgs e)
        {
            if ((sender as FrameworkElement)?.DataContext is not BoostTask task) return;

            var win = new DetailWindow(_dataService, task.Id, readOnly: false) { Owner = this };
            win.ShowDialog();
            Refresh();
        }

        private void Terminate_Click(object sender, RoutedEventArgs e)
        {
            if ((sender as FrameworkElement)?.DataContext is not BoostTask task) return;

            var result = MessageBox.Show(
                $"确定要终止「{task.Title}」吗？\n终止后该板块将不再显示于主界面，但可在历史记录中查看。",
                "确认终止",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            task.Status = BoostStatus.Terminated;
            task.FinishedAt = System.DateTime.Now;
            _dataService.UpdateTask(task);
            Refresh();
        }

        private void Settle_Click(object sender, RoutedEventArgs e)
        {
            if ((sender as FrameworkElement)?.DataContext is not BoostTask task) return;

            var result = MessageBox.Show(
                $"确认「{task.Title}」已经完成结算了吗？",
                "确认结算",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);

            if (result != MessageBoxResult.Yes) return;

            task.Status = BoostStatus.Completed;
            task.FinishedAt = System.DateTime.Now;
            _dataService.UpdateTask(task);
            Refresh();
        }
    }
}
