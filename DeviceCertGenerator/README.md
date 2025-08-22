# Device Certificate Generator for Azure IoT Hub DPS

A Windows application for generating X.509 device certificates for Azure IoT Hub Device Provisioning Service (DPS).

## Features

- **Single Device Processing**: Manually enter an IMEI to generate certificates for individual devices
- **Batch Processing**: Upload CSV or XLSX files containing multiple IMEIs
- **Azure IoT Hub DPS Integration**: Generates certificates compatible with Azure IoT Hub DPS X.509 authentication
- **Progress Tracking**: Real-time progress updates and detailed logging
- **Certificate Chain**: Generates complete certificate chains for device authentication

## Prerequisites

1. **OpenSSL**: Must be installed and available in the system PATH
   - Download from: https://slproweb.com/products/Win32OpenSSL.html
   - Ensure `openssl.exe` is accessible from command line

2. **CA Certificate and Key**: You need your verified CA certificate and private key from Azure DPS
   - `RootCA-Local.pem` (CA certificate)
   - `RootCA-Local.key` (CA private key)

## How to Use

### Single Device
1. Enter a valid IMEI (14-16 digits) in the "Single Device" section
2. Click "Generate Certificate"
3. The application will create certificate files in `.\device\<IMEI>\` folder

### Batch Processing
1. Prepare a CSV or XLSX file with IMEIs
   - CSV: Can have headers, IMEIs in any column
   - XLSX: IMEIs can be in any cell of the first worksheet
2. Click "Browse" to select your file
3. Click "Process Batch"
4. Monitor progress in the log area

### Configuration
- **CA Certificate**: Path to your CA certificate file (default: `.\RootCA-Local.pem`)
- **CA Private Key**: Path to your CA private key file (default: `.\RootCA-Local.key`)

## Output Files

For each device, the application creates:
- `device.key.pem` - Private key (keep secret, install on device)
- `device.cert.pem` - Device certificate (CN = IMEI)
- `device.chain.pem` - Certificate chain (device cert + CA cert)
- `device_cert_log.csv` - Processing log with results

## File Formats

### CSV Format
```csv
IMEI
356789123456789
356789123456790
356789123456791
```

### XLSX Format
IMEIs can be in any cell of the first worksheet. The application will automatically detect and extract valid IMEIs.

## Azure IoT Hub DPS Integration

The generated certificates work with Azure IoT Hub DPS using X.509 authentication:

1. The CN (Common Name) of each certificate is set to the IMEI
2. By default, DPS uses the CN as the DeviceId
3. The certificates are signed by your verified CA
4. Devices can use these certificates to authenticate with DPS and get assigned to an IoT Hub

## Troubleshooting

### OpenSSL Not Found
- Ensure OpenSSL is installed and in your system PATH
- Try the full path: `C:\Program Files\OpenSSL-Win64\bin\openssl.exe`

### Certificate Generation Fails
- Verify CA certificate and key files exist and are valid
- Check file permissions
- Ensure the CA key corresponds to the CA certificate

### Invalid IMEI
- IMEIs must be 14-16 digits
- Only numeric characters are allowed
- The application performs basic validation

## Building from Source

1. Ensure you have .NET 8.0 SDK installed
2. Restore packages: `dotnet restore`
3. Build: `dotnet build`
4. Run: `dotnet run`

## Dependencies

- EPPlus (for Excel file processing)
- CsvHelper (for CSV file processing)
- WPF (.NET 8.0)

## License

This is a proof-of-concept application for internal use.