Below is a drop-in PowerShell script that:

validates the IMEI,

generates a device key,

makes a CSR with CN = IMEI,

signs it with your CA (the same CA you uploaded/verified in DPS),

outputs ready-to-use files (.key, .pem, chain.pem, optional .pfx),

and logs a CSV.

Assumes you’re using the CA you verified with DPS (from your earlier steps that’s likely RootCA-Local.pem + RootCA-Local.key).
Put this script in your working folder (e.g. C:\temp\iot-ca) where your CA files live.

Script: make-device-cert.ps1
<#  make-device-cert.ps1
    Usage (single IMEI):
      .\make-device-cert.ps1 -Imei 356789123456789

    Usage (many IMEIs from CSV, one per line):
      .\make-device-cert.ps1 -InputCsv .\imeis.csv

    Notes:
    - Creates: device\<IMEI>\device.key, device.cert.pem, device.chain.pem, (optional) device.pfx
    - CN of the device certificate == IMEI (DPS will use this as DeviceId)
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory=$false)] [string]$Imei,
  [Parameter(Mandatory=$false)] [string]$InputCsv,
  [string]$OutDir = ".\device",
  # --- point these to the CA you verified in DPS ---
  [string]$CaCert = ".\RootCA-Local.pem",
  [string]$CaKey  = ".\RootCA-Local.key",
  [int]$KeyBits = 2048,
  [int]$Days = 365,
  # set to create a .pfx for each device (leave blank to skip)
  [string]$PfxPassword = ""
)

# --- helpers ---
function Fail($msg){ Write-Error $msg; exit 1 }
function Check-File($p){ if(-not (Test-Path $p)){ Fail "File not found: $p" } }

# prerequisites
Check-File $CaCert
Check-File $CaKey

# collect IMEIs
$imeis = @()
if ($Imei) { $imeis = @($Imei) }
elseif ($InputCsv) {
  if(-not (Test-Path $InputCsv)){ Fail "CSV not found: $InputCsv" }
  $imeis = Get-Content -Raw $InputCsv | Select-String -Pattern '\d+' -AllMatches | ForEach-Object { $_.Matches.Value } | Where-Object { $_ -match '^\d{14,16}$' } | Select-Object -Unique
} else {
  $entered = Read-Host "Enter IMEI (15 digits)"; $imeis = @($entered)
}

if(-not $imeis -or $imeis.Count -eq 0){ Fail "No IMEIs provided." }

# IMEI validator (basic length + numeric; Luhn optional)
function Test-Imei([string]$s){
  if($s -notmatch '^\d{14,16}$'){ return $false }
  # simple: accept 15 (or 14/16 if your estate varies). Uncomment for strict 15:
  # if($s.Length -ne 15){ return $false }
  return $true
}

# ensure OpenSSL is resolvable
$oss = "openssl"
try { & $oss version | Out-Null } catch { Fail "OpenSSL not found on PATH. Try: `C:\Program Files\OpenSSL-Win64\bin\openssl.exe`" }

# ensure out dir
New-Item -Force -ItemType Directory -Path $OutDir | Out-Null

$log = Join-Path $OutDir "device_cert_log.csv"
if(-not (Test-Path $log)) { "imei,device_dir,cert_pem,key_pem,chain_pem,pfx,outcome" | Out-File -Encoding utf8 $log }

foreach($id in $imeis){
  if(-not (Test-Imei $id)){ Write-Warning "Skip invalid IMEI: $id"; continue }

  $devDir = Join-Path $OutDir $id
  New-Item -Force -ItemType Directory -Path $devDir | Out-Null

  $key = Join-Path $devDir "device.key.pem"
  $csr = Join-Path $devDir "device.csr.pem"
  $crt = Join-Path $devDir "device.cert.pem"
  $chn = Join-Path $devDir "device.chain.pem"
  $pfx = Join-Path $devDir "device.pfx"

  Write-Host "==> Generating $id" -ForegroundColor Cyan
  # 1) key
  & $oss genrsa -out $key $KeyBits | Out-Null

  # 2) CSR with CN=IMEI
  & $oss req -new -key $key -out $csr -subj "/CN=$id" | Out-Null

  # 3) Sign with CA
  & $oss x509 -req -in $csr -CA $CaCert -CAkey $CaKey -CAcreateserial -out $crt -days $Days -sha256 | Out-Null

  # 4) Build chain (device then CA). If you use an intermediate CA, put it here instead of $CaCert.
  Get-Content $crt, $CaCert | Set-Content -Encoding ascii $chn

  # 5) Optional PFX for stacks that want a single bundle
  $pfxMade = ""
  if($PfxPassword){
    # Create temp combined PEM for pkcs12 import
    $combo = Join-Path $devDir "combo.pem"
    Get-Content $crt, $key | Set-Content -Encoding ascii $combo
    & $oss pkcs12 -export -inkey $key -in $crt -certfile $CaCert -out $pfx -passout pass:$PfxPassword | Out-Null
    Remove-Item $combo -Force
    if(Test-Path $pfx){ $pfxMade = $pfx }
  }

  # quick sanity
  $ok = Test-Path $key -and Test-Path $crt -and Test-Path $chn
  $status = $ok ? "ok" : "failed"
  "$id,$devDir,$crt,$key,$chn,$pfxMade,$status" | Add-Content -Encoding utf8 $log

  if($ok){
    Write-Host "   • Created: $crt" -ForegroundColor Green
    Write-Host "   • Key:     $key" -ForegroundColor Green
    Write-Host "   • Chain:   $chn" -ForegroundColor Green
    if($pfxMade){ Write-Host "   • PFX:     $pfxMade" -ForegroundColor Green }
  } else {
    Write-Warning "   Generation failed for $id"
  }
}

Write-Host "`nAll done. Log:`n  $log" -ForegroundColor Yellow

What this gives you (per IMEI)

In .\device\<IMEI>\:

device.key.pem → private key (install on the device, keep secret)

device.cert.pem → device certificate (CN = IMEI)

device.chain.pem → device cert + CA cert (use this for TLS chain if your SDK expects it)

device.pfx → optional (only if you provided -PfxPassword)

DPS will accept the device because the cert chains to your verified CA. The CN (IMEI) becomes the DeviceId by default.

Typical ways you’ll run it

Single device (scan one IMEI):

.\make-device-cert.ps1 -Imei 356789123456789 -CaCert .\RootCA-Local.pem -CaKey .\RootCA-Local.key


Batch from file (one IMEI per line):

.\make-device-cert.ps1 -InputCsv .\imeis.txt -CaCert .\RootCA-Local.pem -CaKey .\RootCA-Local.key


Also make PFXs (if your client stack wants .pfx):

.\make-device-cert.ps1 -InputCsv .\imeis.txt -PfxPassword "StrongTempPwd!"

Device client side (what to load)

Client auth cert: device.cert.pem

Private key: device.key.pem

Trusted chain: either device.chain.pem or just the CA (RootCA-Local.pem), depending on your SDK

Connect to DPS using X.509 → device will get assigned to IoT Hub → reuse same cert to talk to the Hub.