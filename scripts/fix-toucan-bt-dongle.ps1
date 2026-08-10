#Requires -RunAsAdministrator
# Fixes Realtek USB BT dongle (VID_0BDA&PID_A760) sleep/reconnect so Windows
# finds ZMK/Toucan without opening the Bluetooth Settings panel (which forces a scan).
$ErrorActionPreference = "Continue"
Write-Host "Fixing Realtek Bluetooth dongle + Windows BT power policy..." -ForegroundColor Cyan

$dongleMatch = 'VID_0BDA&PID_A760'
$dongleId = (Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -match $dongleMatch } |
  Select-Object -First 1 -ExpandProperty InstanceId)

# 1) "Allow computer to turn off this device" — dongle + its USB root hub + BTHUSB
$powerTargets = @($dongleMatch, 'BTHUSB')
if ($dongleId) {
  try {
    $parent = (Get-PnpDeviceProperty -InstanceId $dongleId -KeyName 'DEVPKEY_Device_Parent' -ErrorAction Stop).Data
    if ($parent) {
      Write-Host "  Dongle parent hub: $parent"
      $powerTargets += [regex]::Escape($parent)
    }
  } catch { }
}

Get-CimInstance -ClassName MSPower_DeviceEnable -Namespace root\wmi -ErrorAction SilentlyContinue |
  Where-Object {
    $n = $_.InstanceName
    foreach ($t in $powerTargets) { if ($n -match $t) { return $true } }
    $false
  } |
  ForEach-Object {
    Write-Host ("  Disabling USB power-off: {0}" -f $_.InstanceName)
    $_.Enable = $false
    Set-CimInstance -CimInstance $_
  }

# 2) USB Device Parameters selective-suspend flags for the Realtek stick
$paths = Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Enum\USB" -Recurse -ErrorAction SilentlyContinue |
  Where-Object { $_.PSChildName -eq 'Device Parameters' -and $_.PSPath -match $dongleMatch }
foreach ($dp in $paths) {
  foreach ($name in @(
      'EnhancedPowerManagementEnabled',
      'AllowIdleIrpInD3',
      'DeviceSelectiveSuspended',
      'SelectiveSuspendEnabled',
      'SelectiveSuspendOn',
      'D3ColdSupported'
    )) {
    New-ItemProperty -Path $dp.PSPath -Name $name -PropertyType DWord -Value 0 -Force | Out-Null
  }
  Write-Host ("  USB Device Parameters cleared: {0}" -f $dp.PSPath)
}

# 3) BTHUSB driver selective suspend (host radio idle = no reconnect until Settings opens)
$bthUsb = 'HKLM:\SYSTEM\CurrentControlSet\Services\BTHUSB\Parameters'
if (-not (Test-Path $bthUsb)) { New-Item -Path $bthUsb -Force | Out-Null }
New-ItemProperty -Path $bthUsb -Name 'SelectiveSuspendEnabled' -PropertyType DWord -Value 0 -Force | Out-Null
New-ItemProperty -Path $bthUsb -Name 'SelectiveSuspendOn' -PropertyType DWord -Value 0 -Force | Out-Null
Write-Host "  BTHUSB SelectiveSuspendEnabled=0"

# 4) System USB selective suspend off (AC + DC)
$usbSub = '2a737bb5-1dca-4235-9690-dcc9972f727d'
$selSus = '48e6b7a6-50f5-4782-a5d4-53bb8f07e226'
powercfg /SETACVALUEINDEX SCHEME_CURRENT $usbSub $selSus 0 | Out-Null
powercfg /SETDCVALUEINDEX SCHEME_CURRENT $usbSub $selSus 0 | Out-Null
powercfg /SETACTIVE SCHEME_CURRENT | Out-Null
Write-Host "  powercfg USB selective suspend disabled"

# 5) Keep Bluetooth Support Service available
try {
  Set-Service -Name bthserv -StartupType Automatic -ErrorAction Stop
  Start-Service bthserv -ErrorAction SilentlyContinue
  Write-Host "  bthserv = Automatic"
} catch {
  Write-Host ("  bthserv tweak skipped: {0}" -f $_.Exception.Message)
}

Write-Host "`nVerify MSPower Enable (should be False for dongle/hub):" -ForegroundColor Cyan
Get-CimInstance -ClassName MSPower_DeviceEnable -Namespace root\wmi -ErrorAction SilentlyContinue |
  Where-Object {
    $n = $_.InstanceName
    foreach ($t in $powerTargets) { if ($n -match $t) { return $true } }
    $false
  } |
  Format-Table InstanceName, Enable -AutoSize

Write-Host @"

Done. Next:
  1) Unplug/replug the Realtek dongle (or reboot).
  2) Flash matched left UF2 if you also rebuilt firmware (TX +8 / GATT tweak).
  3) Test: sleep PC or walk away, then wake — Toucan should reconnect without opening Bluetooth Settings.

"@ -ForegroundColor Green
