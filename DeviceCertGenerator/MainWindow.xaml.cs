using Microsoft.Win32;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows;
using System.Diagnostics;
using CsvHelper;
using OfficeOpenXml;
using System.Globalization;

namespace DeviceCertGenerator
{
    public partial class MainWindow : Window
    {
        private const string DEFAULT_OUTPUT_DIR = ".\\device";
        private const int DEFAULT_KEY_BITS = 2048;
        private const int DEFAULT_DAYS = 365;

        public MainWindow()
        {
            InitializeComponent();
            ExcelPackage.LicenseContext = LicenseContext.NonCommercial;
            LogMessage("Application started. Ready to generate device certificates.");
        }

        private void ProcessSingleButton_Click(object sender, RoutedEventArgs e)
        {
            var imei = ImeiTextBox.Text.Trim();
            if (string.IsNullOrEmpty(imei))
            {
                MessageBox.Show("Please enter an IMEI.", "Input Required", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (!ValidateImei(imei))
            {
                MessageBox.Show("Invalid IMEI. Please enter 14-16 digits.", "Invalid Input", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            ProcessDevices(new List<string> { imei });
        }

        private void BrowseButton_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "CSV files (*.csv)|*.csv|Excel files (*.xlsx)|*.xlsx|All files (*.*)|*.*",
                FilterIndex = 1
            };

            if (dialog.ShowDialog() == true)
            {
                FilePathTextBox.Text = dialog.FileName;
            }
        }

        private void ProcessBatchButton_Click(object sender, RoutedEventArgs e)
        {
            var filePath = FilePathTextBox.Text.Trim();
            if (string.IsNullOrEmpty(filePath))
            {
                MessageBox.Show("Please select a file.", "File Required", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (!File.Exists(filePath))
            {
                MessageBox.Show("Selected file does not exist.", "File Not Found", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            try
            {
                var imeis = LoadImeisFromFile(filePath);
                if (imeis.Count == 0)
                {
                    MessageBox.Show("No valid IMEIs found in the file.", "No Data", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                ProcessDevices(imeis);
            }
            catch (Exception ex)
            {
                LogMessage($"Error loading file: {ex.Message}");
                MessageBox.Show($"Error loading file: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BrowseCaCertButton_Click(object sender, RoutedEventArgs e)
        {
            BrowseForFile(CaCertPathTextBox, "Certificate files (*.pem)|*.pem|All files (*.*)|*.*");
        }

        private void BrowseCaKeyButton_Click(object sender, RoutedEventArgs e)
        {
            BrowseForFile(CaKeyPathTextBox, "Key files (*.key)|*.key|All files (*.*)|*.*");
        }

        private void BrowseForFile(System.Windows.Controls.TextBox textBox, string filter)
        {
            var dialog = new OpenFileDialog { Filter = filter };
            if (dialog.ShowDialog() == true)
            {
                textBox.Text = dialog.FileName;
            }
        }

        private bool ValidateImei(string imei)
        {
            return Regex.IsMatch(imei, @"^\d{14,16}$");
        }

        private List<string> LoadImeisFromFile(string filePath)
        {
            var imeis = new List<string>();
            var extension = Path.GetExtension(filePath).ToLower();

            if (extension == ".csv")
            {
                imeis = LoadImeisFromCsv(filePath);
            }
            else if (extension == ".xlsx")
            {
                imeis = LoadImeisFromExcel(filePath);
            }
            else
            {
                throw new ArgumentException("Unsupported file format. Please use CSV or XLSX files.");
            }

            return imeis.Where(ValidateImei).Distinct().ToList();
        }

        private List<string> LoadImeisFromCsv(string filePath)
        {
            var imeis = new List<string>();
            
            using var reader = new StringReader(File.ReadAllText(filePath));
            using var csv = new CsvReader(reader, CultureInfo.InvariantCulture);
            
            csv.Read();
            csv.ReadHeader();
            
            while (csv.Read())
            {
                for (int i = 0; i < csv.HeaderRecord?.Length; i++)
                {
                    var value = csv.GetField(i)?.Trim();
                    if (!string.IsNullOrEmpty(value) && Regex.IsMatch(value, @"\d{14,16}"))
                    {
                        imeis.Add(value);
                    }
                }
            }

            if (imeis.Count == 0)
            {
                var lines = File.ReadAllLines(filePath);
                foreach (var line in lines)
                {
                    var matches = Regex.Matches(line, @"\d{14,16}");
                    foreach (Match match in matches)
                    {
                        imeis.Add(match.Value);
                    }
                }
            }

            return imeis;
        }

        private List<string> LoadImeisFromExcel(string filePath)
        {
            var imeis = new List<string>();
            
            using var package = new ExcelPackage(new FileInfo(filePath));
            var worksheet = package.Workbook.Worksheets[0];
            
            for (int row = 1; row <= worksheet.Dimension.Rows; row++)
            {
                for (int col = 1; col <= worksheet.Dimension.Columns; col++)
                {
                    var cellValue = worksheet.Cells[row, col].Text?.Trim();
                    if (!string.IsNullOrEmpty(cellValue) && Regex.IsMatch(cellValue, @"\d{14,16}"))
                    {
                        imeis.Add(cellValue);
                    }
                }
            }

            return imeis;
        }

        private async void ProcessDevices(List<string> imeis)
        {
            if (!ValidateConfiguration())
                return;

            SetControlsEnabled(false);
            ProgressBar.Maximum = imeis.Count;
            ProgressBar.Value = 0;

            var outputDir = DEFAULT_OUTPUT_DIR;
            Directory.CreateDirectory(outputDir);

            var logFilePath = Path.Combine(outputDir, "device_cert_log.csv");
            if (!File.Exists(logFilePath))
            {
                await File.WriteAllTextAsync(logFilePath, "imei,device_dir,cert_pem,key_pem,chain_pem,outcome\n");
            }

            LogMessage($"Starting certificate generation for {imeis.Count} device(s)...");

            int successCount = 0;
            int failureCount = 0;

            foreach (var imei in imeis)
            {
                try
                {
                    UpdateStatus($"Processing {imei}...");
                    LogMessage($"Processing IMEI: {imei}");

                    var result = await GenerateDeviceCertificate(imei, outputDir);
                    
                    var logEntry = $"{imei},{result.DeviceDir},{result.CertPath},{result.KeyPath},{result.ChainPath},{(result.Success ? "ok" : "failed")}\n";
                    await File.AppendAllTextAsync(logFilePath, logEntry);

                    if (result.Success)
                    {
                        successCount++;
                        LogMessage($"✓ Generated certificate for {imei}");
                    }
                    else
                    {
                        failureCount++;
                        LogMessage($"✗ Failed to generate certificate for {imei}: {result.ErrorMessage}");
                    }
                }
                catch (Exception ex)
                {
                    failureCount++;
                    LogMessage($"✗ Error processing {imei}: {ex.Message}");
                }

                ProgressBar.Value++;
                await Task.Delay(100);
            }

            LogMessage($"Certificate generation completed. Success: {successCount}, Failures: {failureCount}");
            UpdateStatus($"Completed. Success: {successCount}, Failures: {failureCount}");
            SetControlsEnabled(true);

            if (successCount > 0)
            {
                var result = MessageBox.Show($"Certificate generation completed!\n\nSuccess: {successCount}\nFailures: {failureCount}\n\nWould you like to open the output directory?", 
                    "Process Complete", MessageBoxButton.YesNo, MessageBoxImage.Information);
                
                if (result == MessageBoxResult.Yes)
                {
                    Process.Start("explorer.exe", Path.GetFullPath(outputDir));
                }
            }
        }

        private bool ValidateConfiguration()
        {
            var caCertPath = CaCertPathTextBox.Text.Trim();
            var caKeyPath = CaKeyPathTextBox.Text.Trim();

            if (string.IsNullOrEmpty(caCertPath) || !File.Exists(caCertPath))
            {
                MessageBox.Show("Please specify a valid CA certificate file.", "Configuration Error", MessageBoxButton.OK, MessageBoxImage.Warning);
                return false;
            }

            if (string.IsNullOrEmpty(caKeyPath) || !File.Exists(caKeyPath))
            {
                MessageBox.Show("Please specify a valid CA private key file.", "Configuration Error", MessageBoxButton.OK, MessageBoxImage.Warning);
                return false;
            }

            if (!IsOpenSslAvailable())
            {
                MessageBox.Show("OpenSSL is not available on the system PATH. Please install OpenSSL and ensure it's accessible.", 
                    "OpenSSL Required", MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }

            return true;
        }

        private bool IsOpenSslAvailable()
        {
            try
            {
                using var process = new Process();
                process.StartInfo.FileName = "openssl";
                process.StartInfo.Arguments = "version";
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.CreateNoWindow = true;
                process.Start();
                process.WaitForExit();
                return process.ExitCode == 0;
            }
            catch
            {
                return false;
            }
        }

        private async Task<CertificateGenerationResult> GenerateDeviceCertificate(string imei, string outputDir)
        {
            var result = new CertificateGenerationResult();
            
            try
            {
                var deviceDir = Path.Combine(outputDir, imei);
                Directory.CreateDirectory(deviceDir);

                var keyPath = Path.Combine(deviceDir, "device.key.pem");
                var csrPath = Path.Combine(deviceDir, "device.csr.pem");
                var certPath = Path.Combine(deviceDir, "device.cert.pem");
                var chainPath = Path.Combine(deviceDir, "device.chain.pem");

                result.DeviceDir = deviceDir;
                result.KeyPath = keyPath;
                result.CertPath = certPath;
                result.ChainPath = chainPath;

                var caCertPath = CaCertPathTextBox.Text.Trim();
                var caKeyPath = CaKeyPathTextBox.Text.Trim();

                await RunOpenSslCommand($"genrsa -out \"{keyPath}\" {DEFAULT_KEY_BITS}");

                await RunOpenSslCommand($"req -new -key \"{keyPath}\" -out \"{csrPath}\" -subj \"/CN={imei}\"");

                await RunOpenSslCommand($"x509 -req -in \"{csrPath}\" -CA \"{caCertPath}\" -CAkey \"{caKeyPath}\" -CAcreateserial -out \"{certPath}\" -days {DEFAULT_DAYS} -sha256");

                var certContent = await File.ReadAllTextAsync(certPath);
                var caCertContent = await File.ReadAllTextAsync(caCertPath);
                await File.WriteAllTextAsync(chainPath, certContent + caCertContent);

                File.Delete(csrPath);

                result.Success = File.Exists(keyPath) && File.Exists(certPath) && File.Exists(chainPath);
            }
            catch (Exception ex)
            {
                result.Success = false;
                result.ErrorMessage = ex.Message;
            }

            return result;
        }

        private async Task<bool> RunOpenSslCommand(string arguments)
        {
            using var process = new Process();
            process.StartInfo.FileName = "openssl";
            process.StartInfo.Arguments = arguments;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.CreateNoWindow = true;
            process.StartInfo.RedirectStandardError = true;
            
            process.Start();
            await process.WaitForExitAsync();
            
            if (process.ExitCode != 0)
            {
                var error = await process.StandardError.ReadToEndAsync();
                throw new Exception($"OpenSSL command failed: {error}");
            }
            
            return true;
        }

        private void SetControlsEnabled(bool enabled)
        {
            ProcessSingleButton.IsEnabled = enabled;
            ProcessBatchButton.IsEnabled = enabled;
            BrowseButton.IsEnabled = enabled;
        }

        private void UpdateStatus(string message)
        {
            StatusTextBlock.Text = message;
        }

        private void LogMessage(string message)
        {
            var timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
            var logEntry = $"[{timestamp}] {message}\n";
            
            Dispatcher.Invoke(() =>
            {
                LogTextBox.AppendText(logEntry);
                LogTextBox.ScrollToEnd();
            });
        }

        private class CertificateGenerationResult
        {
            public bool Success { get; set; }
            public string DeviceDir { get; set; } = string.Empty;
            public string KeyPath { get; set; } = string.Empty;
            public string CertPath { get; set; } = string.Empty;
            public string ChainPath { get; set; } = string.Empty;
            public string ErrorMessage { get; set; } = string.Empty;
        }
    }
}