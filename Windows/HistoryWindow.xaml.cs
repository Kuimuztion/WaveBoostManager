using System.Windows;
using WaveBoostManager.Models;
using WaveBoostManager.Services;

namespace WaveBoostManager.Windows
{
    public partial class HistoryWindow : Window
    {
        private readonly DataService _dataService;

        public HistoryWindow(DataService dataService)
        {
            InitializeComponent();
            _dataService = dataService;
            Refresh();
        }

        private void Refresh()
        {
            var history = _dataService.GetHistory();
            HistoryList.ItemsSource = history;
            TxtEmptyHint.Visibility = history.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        private void View_Click(object sender, RoutedEventArgs e)
        {
            if ((sender as FrameworkElement)?.DataContext is not BoostTask task) return;

            var win = new DetailWindow(_dataService, task.Id, readOnly: true) { Owner = this };
            win.ShowDialog();
        }
    }
}
