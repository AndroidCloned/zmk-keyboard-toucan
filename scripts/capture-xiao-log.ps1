# Capture ZMK USB CDC logs from the Seeed XIAO on COM4.
# Usage (PowerShell):
#   powershell -ExecutionPolicy Bypass -File .\capture-xiao-log.ps1
# Optional:
#   .\capture-xiao-log.ps1 -Port COM4 -Seconds 30 -OutFile C:\Users\5950x\Desktop\toucan-xiao.log

param(
    [string]$Port = "COM4",
    [int]$Seconds = 25,
    [int]$Baud = 115200,
    [string]$OutFile = "C:\Users\5950x\Desktop\toucan-xiao.log"
)

$ErrorActionPreference = "Stop"
Write-Host "Opening $Port at $Baud for $Seconds seconds..."
Write-Host "Tip: plug USB now / reset the board within the first few seconds."

$portObj = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
$portObj.ReadTimeout = 500
$portObj.WriteTimeout = 500
$portObj.DtrEnable = $true
$portObj.RtsEnable = $true
$portObj.NewLine = "`n"

try {
    $portObj.Open()
} catch {
    Write-Error "Failed to open $Port. Is another app using it? $_"
    exit 1
}

$buf = New-Object System.Text.StringBuilder
$deadline = (Get-Date).AddSeconds($Seconds)
try {
    while ((Get-Date) -lt $deadline) {
        try {
            $chunk = $portObj.ReadExisting()
            if ($chunk) {
                [void]$buf.Append($chunk)
                Write-Host -NoNewline $chunk
            }
        } catch {
            # read timeout is normal
        }
        Start-Sleep -Milliseconds 50
    }
} finally {
    if ($portObj.IsOpen) { $portObj.Close() }
    $portObj.Dispose()
}

$text = $buf.ToString()
Set-Content -Path $OutFile -Value $text -Encoding UTF8
Write-Host ""
Write-Host "Wrote $($text.Length) chars to $OutFile"
if ([string]::IsNullOrWhiteSpace($text)) {
    Write-Host "WARNING: no serial data received. Confirm you flashed toucan_left_usb_logging.uf2 and COM port is correct."
    exit 2
}
