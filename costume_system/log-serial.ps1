param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [ValidateRange(0,2147483647)][int]$Seconds = 60,
    [switch]$Reconnect
)

$ErrorActionPreference = 'Stop'
$logFolder = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force -Path $logFolder | Out-Null
$logPath = Join-Path $logFolder ('costume-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
$serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, 'None', 8, 'One')
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 500
# Preserve every received byte, including corrupted serial characters.
$serial.Encoding = [System.Text.Encoding]::GetEncoding(28591)
$writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
$writer.AutoFlush = $true
$waitingReported = $false
try {
    $writer.WriteLine("USB capture started $(Get-Date -Format o) | $Port | $Baud baud | reconnect=$Reconnect")
    Write-Host "Log file: $logPath"
    Write-Host 'Close Serial Monitor/Plotter. Stop this logger with Ctrl+C before uploading.'
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($Seconds -eq 0 -or $timer.Elapsed.TotalSeconds -lt $Seconds) {
        if (-not $serial.IsOpen) {
            try {
                $serial.Open()
                $writer.WriteLine("`nUSB_CONNECTED $(Get-Date -Format o) | $Port")
                Write-Host "Recording $Port"
                $waitingReported = $false
            } catch {
                if (-not $Reconnect) { throw }
                if (-not $waitingReported) {
                    $writer.WriteLine("USB_WAIT $(Get-Date -Format o) | $($_.Exception.Message)")
                    Write-Host "Waiting for $Port to be connected and available..."
                    $waitingReported = $true
                }
                Start-Sleep -Milliseconds 500
                continue
            }
        }
        try {
            $chunk = $serial.ReadExisting()
            if ($chunk.Length -gt 0) {
                $writer.Write($chunk)
                [Console]::Write($chunk)
            }
        } catch {
            if (-not $Reconnect) { throw }
            $writer.WriteLine("`nUSB_DISCONNECTED $(Get-Date -Format o) | $($_.Exception.Message)")
            try { $serial.Close() } catch {}
        }
        Start-Sleep -Milliseconds 20
    }
} finally {
    $writer.WriteLine("`nUSB_CAPTURE_STOPPED $(Get-Date -Format o)")
    $writer.Dispose()
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
