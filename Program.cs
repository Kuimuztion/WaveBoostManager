using System;
using System.IO;
using System.Windows;

namespace WaveBoostManager
{
    public static class Program
    {
        [STAThread]
        public static int Main()
        {
            try
            {
                var app = new App();
                app.InitializeComponent();
                return app.Run();
            }
            catch (Exception ex)
            {
                WriteCrashLog(ex);

                // 这里用最原始的方式弹窗，不依赖任何 XAML/资源，保证一定能显示出来
                MessageBox.Show(
                    "程序启动失败：\n\n" + ex + "\n\n详细信息已写入程序目录下的 crash_log.txt",
                    "启动失败",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);

                return -1;
            }
        }

        private static void WriteCrashLog(Exception ex)
        {
            try
            {
                var logPath = Path.Combine(AppContext.BaseDirectory, "crash_log.txt");
                File.AppendAllText(logPath, $"[{DateTime.Now}]\n{ex}\n\n");
            }
            catch
            {
                // 日志写入失败也不要再抛异常
            }
        }
    }
}
