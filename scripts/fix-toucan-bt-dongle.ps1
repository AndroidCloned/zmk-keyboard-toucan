# Run as Administrator — fixes Realtek USB BT dongle (VID_0BDA&PID_A760) sleep/reconnect
$ErrorActionPreference = "Continue"
Write-Host "Fixing Realtek Bluetooth dongle power management..." -ForegroundColor Cyan

Get-CimInstance -ClassName MSPower_DeviceEnable -Namespace root\wmi |
  Where-Object { $_.InstanceName -match 'VID_0BDA&PID_A760|BTHUSB' } |
  ForEach-Object {
    Write-Host ("  Disabling USB power-off: {0}" -f $_.InstanceName)
    $_.Enable = $false
    Set-CimInstance -CimInstance $_
  }

$paths = Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Enum\USB" -Recurse -ErrorAction SilentlyContinue |
  Where-Object { $_.PSChildName -eq 'Device Parameters' -and $_.PSPath -match 'VID_0BDA&PID_A760' }
foreach ($dp in $paths) {
  foreach ($name in @('EnhancedPowerManagementEnabled','AllowIdleIrpInD3','DeviceSelectiveSuspended','SelectiveSuspendEnabled','SelectiveSuspendOn')) {
    New-ItemProperty -Path $dp.PSPath -Name $name -PropertyType DWord -Value 0 -Force | Out-Null
  }
  Write-Host ("  Registry cleared: {0}" -f $dp.PSPath)
}

powercfg /SETACVALUEINDEX SCHEME_CURRENT 2a737bb5-1dca-4235-9690-dcc9972f727d 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0
powercfg /SETDCVALUEINDEX SCHEME_CURRENT 2a737bb5-1dca-4235-9690-dcc9972f727d 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0
powercfg /SETACTIVE SCHEME_CURRENT

Write-Host "`nVerify:" -ForegroundColor Cyan
Get-CimInstance -ClassName MSPower_DeviceEnable -Namespace root\wmi |
  Where-Object { $_.InstanceName -match 'VID_0BDA&PID_A760|BTHUSB' } |
  Format-Table InstanceName, Enable -AutoSize

Write-Host "Done. Unplug/replug the dongle (or reboot), then flash toucan_left_no_studio.uf2 and re-pair if needed." -ForegroundColor Green
pause
