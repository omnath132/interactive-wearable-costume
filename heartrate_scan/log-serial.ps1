param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [int]$Seconds = 60
)

$ErrorActionPreference = 'Stop'
$logFolder = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force -Path $logFolder | Out-Null
$logPath = Join-Path $logFolder ('heartrate-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
$serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, 'None', 8, 'One')
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 500
# Preserve every received byte, including corrupted serial characters.
$serial.Encoding = [System.Text.Encoding]::GetEncoding(28591)
$writer = $null
try {
    $serial.Open()
    $writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    $writer.WriteLine("Serial capture started $(Get-Date -Format o) | $Port | $Baud baud")
    Write-Host "Logging to $logPath"
    Write-Host 'Close Serial Monitor before logging. Press Ctrl+C to stop; -Seconds 0 runs continuously.'
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($Seconds -le 0 -or $timer.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $serial.ReadExisting()
        if ($chunk.Length -gt 0) {
            $writer.Write($chunk)
            [Console]::Write($chunk)
        }
        Start-Sleep -Milliseconds 20
    }
} finally {
    if ($null -ne $writer) { $writer.Dispose() }
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
