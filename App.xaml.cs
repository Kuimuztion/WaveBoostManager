using System;
using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace WaveBoostManager
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // 先挂好全局异常捕获，再创建主窗口，这样启动过程中任何报错都能弹窗看到，
            // 而不是像之前那样"没有任何反应"地静默退出。
            AppDomain.CurrentDomain.UnhandledException += OnDomainUnhandledException;
            DispatcherUnhandledException += OnDispatcherUnhandledException;

            base.OnStartup(e);

            try
            {
                var main = new MainWindow();
                main.Show();
            }
            catch (Exception ex)
            {
                ShowCrash(ex);
                Shutdown();
            }
        }

        private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            ShowCrash(e.Exception);
            e.Handled = true; // 弹完窗后不让系统直接杀掉进程
        }

        private void OnDomainUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            if (e.ExceptionObject is Exception ex)
                ShowCrash(ex);
        }

        private void ShowCrash(Exception ex)
        {
            try
            {
                var logPath = Path.Combine(AppContext.BaseDirectory, "crash_log.txt");
                File.AppendAllText(logPath, $"[{DateTime.Now}]\n{ex}\n\n");
            }
            catch
            {
                // 日志写失败也不要再抛异常
            }

            MessageBox.Show(
                "程序遇到错误：\n\n" + ex.Message + "\n\n详细堆栈已写入 crash_log.txt（在程序所在目录）。",
                "出错了",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }

        public bool IsDarkTheme { get; private set; } = true;

        public void ToggleTheme()
        {
            IsDarkTheme = !IsDarkTheme;
            var r = Resources;
            r["BgBrush"] = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(
                IsDarkTheme ? (byte)0x0F : (byte)0xF5,
                IsDarkTheme ? (byte)0x12 : (byte)0xF7,
                IsDarkTheme ? (byte)0x20 : (byte)0xFA));
            r["PanelBrush"] = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(
                IsDarkTheme ? (byte)0x16 : (byte)0xFF,
                IsDarkTheme ? (byte)0x1A : (byte)0xFF,
                IsDarkTheme ? (byte)0x2E : (byte)0xFF));
            r["CardBrush"] = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(
                IsDarkTheme ? (byte)0x1A : (byte)0xFF,
                IsDarkTheme ? (byte)0x1E : (byte)0xFF,
                IsDarkTheme ? (byte)0x33 : (byte)0xFF));
            r["InputBgBrush"] = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(
                IsDarkTheme ? (byte)0x20 : (byte)0xFF,
                IsDarkTheme ? (byte)0x24 : (byte)0xFF,
                IsDarkTheme ? (byte)0x3B : (byte)0xFF));
            r["TextPrimaryBrush"] = new System.Windows.Media.SolidColorBrush(IsDarkTheme ? System.Windows.Media.Colors.WhiteSmoke : System.Windows.Media.Colors.Black);
            r["TextSecondaryBrush"] = new System.Windows.Media.SolidColorBrush(IsDarkTheme ? System.Windows.Media.Colors.Gray : System.Windows.Media.Colors.DimGray);
        }
    }
}
