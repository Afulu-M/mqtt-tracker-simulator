# DPS Integration Testing Guide

## Overview
This guide provides comprehensive testing procedures for the Azure Device Provisioning Service (DPS) integration with X.509 certificate-based authentication.

## Prerequisites

### 1. Certificate Setup
Ensure you have valid device certificates generated:
```
DeviceCertGenerator/device/{IMEI}/
├── device.cert.pem    # Device certificate
├── device.key.pem     # Device private key
└── device.chain.pem   # Certificate chain
```

### 2. Root CA Certificate
Ensure the root CA certificate is available:
```
DeviceCertGenerator/cert/RootCA-2025.pem
```

### 3. Configuration
Update `simulator.toml` with your DPS settings:
```toml
[dps]
id_scope = "0ne00A12345"  # Your actual DPS ID Scope
imei = "89612345678912"   # Device IMEI matching your certificate
device_cert_base_path = "DeviceCertGenerator/device"
root_ca_path = "DeviceCertGenerator/cert/RootCA-2025.pem"
verify_server_cert = true
```

## Testing Procedures

### Test 1: Basic DPS Connection
**Objective**: Verify DPS can provision the device and assign it to an IoT Hub.

**Steps**:
1. Build the project:
   ```bash
   cmake --build build --config Release
   ```

2. Run the CLI application:
   ```bash
   ./build/Release/sim-cli.exe
   ```

3. **Expected Output**:
   ```
   [Simulator] Using DPS for connection
   [DPS] Sent registration request for device: 89612345678912
   [DPS] Received message on topic: $dps/registrations/res/202
   [DPS] Device assignment in progress, operation ID: 4.7f4c...
   [DPS] Successfully provisioned device SIM-001 to hub example.azure-devices.net
   [Simulator] Connected via DPS to hub: example.azure-devices.net as device: SIM-001
   ```

**Pass Criteria**:
- ✅ Connection to DPS established
- ✅ Registration request sent successfully
- ✅ Device successfully assigned to IoT Hub
- ✅ MQTT connection to assigned hub successful

### Test 2: Certificate Validation
**Objective**: Ensure proper certificate validation and error handling.

**Steps**:
1. Test with invalid certificate path:
   ```toml
   device_cert_base_path = "invalid/path"
   ```

2. Run the application and verify error handling:
   ```bash
   ./build/Release/sim-cli.exe
   ```

3. **Expected Output**:
   ```
   [Error] Device certificate not found: invalid/path/89612345678912/device.cert.pem
   [Simulator] DPS connection failed: Invalid certificate paths
   ```

**Pass Criteria**:
- ✅ Clear error messages for missing certificates
- ✅ Application doesn't crash with invalid paths
- ✅ Graceful fallback or error reporting

### Test 3: Event Publishing via DPS
**Objective**: Verify telemetry messages are published through DPS-connected IoT Hub.

**Steps**:
1. Ensure valid DPS configuration
2. Run the application and trigger events:
   ```bash
   ./build/Release/sim-cli.exe --spike 5
   ```

3. **Expected Output**:
   ```
   === EVENT GENERATED ===
   Type: motion_start
   Sequence: 1
   Timestamp: 2025-08-21T14:30:25Z
   📤 Publishing to topic: devices/SIM-001/messages/events/
   ✅ Published to Azure IoT Hub
   ```

**Pass Criteria**:
- ✅ Events generate correctly
- ✅ JSON payload format is correct
- ✅ Messages publish successfully to IoT Hub
- ✅ Device-to-cloud topic format is correct

### Test 4: Legacy Fallback
**Objective**: Verify backward compatibility with SAS token authentication.

**Steps**:
1. Configure with legacy connection string:
   ```toml
   [connection]
   connection_string = "HostName=test.azure-devices.net;DeviceId=TEST-001;SharedAccessKey=ABC123..."
   ```

2. Remove or comment out DPS configuration
3. Run the application

**Pass Criteria**:
- ✅ Falls back to SAS token authentication
- ✅ Connects successfully with legacy method
- ✅ Events publish correctly

### Test 5: Network Error Handling
**Objective**: Test resilience during network failures and reconnection.

**Steps**:
1. Start the application with valid DPS config
2. Disconnect network during operation
3. Reconnect network and verify recovery

**Pass Criteria**:
- ✅ Handles network disconnection gracefully
- ✅ Automatically attempts reconnection
- ✅ Resumes normal operation after reconnection
- ✅ Queued messages are delivered when reconnected

## Monitoring and Diagnostics

### 1. Azure IoT Hub Monitoring
Monitor device activity in Azure Portal:
- Go to IoT Hub → Devices → Select your device
- Check "Device twin" for provisioning status
- Monitor "Device-to-cloud messages" for telemetry

### 2. DPS Monitoring  
Monitor provisioning activities:
- Go to DPS → Device registrations
- Check registration status and assigned hub
- Review provisioning logs

### 3. Application Logs
Key log patterns to monitor:
```
[DPS] - DPS-specific operations
[Simulator] - Main application flow
[Error] - Error conditions
📤/✅/❌ - Message publishing status
```

## Troubleshooting

### Common Issues

#### 1. "Device certificate not found"
**Solution**: 
- Verify certificate files exist in correct location
- Check file permissions
- Ensure IMEI in config matches certificate folder name

#### 2. "Failed to connect to DPS"
**Solutions**:
- Verify network connectivity to `global.azure-devices-provisioning.net:8883`
- Check firewall settings
- Validate certificate chain and root CA

#### 3. "Registration failed"
**Solutions**:
- Verify DPS ID Scope is correct
- Check device is enrolled in DPS
- Validate X.509 certificate is properly configured in DPS enrollment

#### 4. "Assignment timeout"
**Solutions**:
- Check IoT Hub has capacity for new devices
- Verify DPS allocation policy settings
- Review DPS enrollment group configuration

### Debug Mode
Enable verbose logging by modifying the application or adding debug output.

## Performance Metrics

Monitor these metrics during testing:

| Metric | Expected Value | Notes |
|--------|---------------|-------|
| DPS Connection Time | < 30 seconds | Initial provisioning |
| Hub Connection Time | < 10 seconds | After DPS assignment |
| Message Publish Rate | 100+ msg/min | Depends on heartbeat config |
| Memory Usage | < 50MB | Desktop application |
| Reconnection Time | < 60 seconds | After network recovery |

## Certification Requirements

For production deployment, ensure:
- [ ] Certificates are properly secured
- [ ] Private keys are protected
- [ ] Server certificate validation is enabled
- [ ] Network timeouts are appropriately configured
- [ ] Error logging is comprehensive
- [ ] Memory leaks are checked with long-running tests

## Security Validation

Verify these security aspects:
- [ ] TLS 1.2+ is used for all connections
- [ ] Certificate validation is enabled
- [ ] No sensitive data in logs
- [ ] Private keys are not exposed
- [ ] Connections fail with invalid certificates

## Next Steps

After successful testing:
1. Deploy to test environment with real Azure DPS
2. Perform load testing with multiple devices
3. Test with different certificate authorities
4. Validate against IoT Hub quotas and limits
5. Prepare for embedded target compilation (STM32H)

## Contact and Support

For issues with this integration:
- Review Azure DPS documentation
- Check MQTT client library documentation (Paho)
- Validate certificate generation process
- Test with Azure IoT tools and CLI