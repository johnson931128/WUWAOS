[CmdletBinding()]
param(
    [string]$Distro = 'Ubuntu',
    [string]$ViewerPath,
    [ValidateRange(5, 300)][int]$StartupTimeoutSeconds = 60,
    [switch]$Snapshot
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $repoRoot 'build'
$baseDisk = Join-Path $buildDir 'wuwaos.img'
$baseSwap = Join-Path $buildDir 'wuwaos-swap.img'
$viewerCandidates = @()
$sessionId = [Guid]::NewGuid().ToString('N')
$sessionDir = Join-Path $buildDir "desktop-$sessionId"
$lockPath = Join-Path $buildDir 'desktop-launch.lock'
$lockStream = $null
$buildProcess = $null
$qemuProcess = $null
$viewerProcess = $null
$qmpClient = $null
$runnerPath = Join-Path $sessionDir 'run.sh'
$buildRunnerPath = Join-Path $sessionDir 'build.sh'
$stopPath = Join-Path $sessionDir 'stop.sh'
$pidFile = Join-Path $sessionDir 'qemu.pid'
$stopWsl = $null
$vmName = "wuwaos-$sessionId"
$ownsQemu = $false
$qemuStopped = $true
$snapshotPathsOwned = $false
$diskPath = $baseDisk
$swapPath = $baseSwap
$snapshotDiskPath = Join-Path $sessionDir 'wuwaos.img'
$snapshotSwapPath = Join-Path $sessionDir 'wuwaos-swap.img'
if ($Snapshot) {
    $diskPath = $snapshotDiskPath
    $swapPath = $snapshotSwapPath
}
$vncDisplay = 0
$qmpPort = 0

function Find-Viewer {
    if ($ViewerPath) {
        if (-not (Test-Path -LiteralPath $ViewerPath -PathType Leaf)) {
            throw "VNC viewer not found at -ViewerPath '$ViewerPath'."
        }
        return (Resolve-Path -LiteralPath $ViewerPath).Path
    }
    $cmd = Get-Command vncviewer.exe, vncviewer -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
    $viewerCandidates = @(
        (Join-Path $repoRoot 'build/tigervnc/vncviewer64-1.16.2.exe'),
        (Join-Path $repoRoot 'build/tigervnc/vncviewer64.exe'),
        (Join-Path $env:ProgramFiles 'TigerVNC/vncviewer.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'TigerVNC/vncviewer.exe'),
        (Join-Path $env:ProgramFiles 'TightVNC/tvnviewer.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'TightVNC/tvnviewer.exe')
    )
    foreach ($candidate in $viewerCandidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    throw "No Windows VNC viewer found. Install TigerVNC from https://github.com/TigerVNC/tigervnc/releases, then rerun with -ViewerPath 'C:\path\to\vncviewer64.exe'."
}

function Test-PortFree([int]$Port) {
    $listener = $null
    try {
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
        $listener.Start()
        return $true
    } catch {
        return $false
    } finally {
        if ($listener) { $listener.Stop() }
    }
}

function Select-FreePort([int]$Minimum, [int]$Maximum) {
    for ($attempt = 0; $attempt -lt 200; $attempt++) {
        $candidate = Get-Random -Minimum $Minimum -Maximum ($Maximum + 1)
        if (Test-PortFree $candidate) { return $candidate }
    }
    throw "Could not find an unused loopback port in $Minimum..$Maximum."
}

function Escape-Bash([string]$Value) {
    return "'" + $Value.Replace("'", "'\''") + "'"
}

function Quote-WindowsArgument([string]$Value) {
    $escaped = $Value -replace '(\\*)"', '$1$1\"'
    $escaped = $escaped -replace '(\\+)$', '$1$1'
    return '"' + $escaped + '"'
}

function Convert-ToWslPath([string]$Path) {
    $wslCompatiblePath = $Path.Replace('\', '/')
    $mapped = & wsl.exe -d $Distro -- wslpath -u $wslCompatiblePath 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Cannot map '$Path' into WSL: $($mapped -join ' ')" }
    return ($mapped | Select-Object -Last 1).Trim()
}

function Send-Qmp([string]$Command) {
    $payload = @{ execute = $Command } | ConvertTo-Json -Compress
    $bytes = [Text.Encoding]::UTF8.GetBytes($payload + "`r`n")
    $qmpClient.GetStream().Write($bytes, 0, $bytes.Length)
    while ($true) {
        $line = $qmpReader.ReadLine()
        if (-not $line) { throw "QMP closed while processing '$Command'." }
        $response = $line | ConvertFrom-Json
        if ($response.event) { continue }
        if ($response.error) { throw "QMP '$Command' failed: $($response.error.desc)" }
        return $response
    }
}

try {
    $viewer = Find-Viewer
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if (-not $wsl) { throw 'wsl.exe is not available on PATH.' }

    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    try {
        $lockStream = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    } catch {
        throw "Another desktop launcher owns '$lockPath' (or a stale lock exists); refusing to start a second session."
    }
    $lockInfo = [Text.Encoding]::UTF8.GetBytes("$PID $sessionId")
    $lockStream.SetLength(0)
    $lockStream.Position = 0
    $lockStream.Write($lockInfo, 0, $lockInfo.Length)
    $lockStream.Flush()

    # This is the requested distribution, so mapping the checkout may start it.
    $wslRoot = Convert-ToWslPath $repoRoot

    # Refuse to rebuild either image while any running WSL distro has QEMU using this checkout.
    $runningDistros = @(& wsl.exe --list --running --quiet 2>$null) -replace "`0", '' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
    foreach ($runningDistro in $runningDistros) {
        $processLines = @(& wsl.exe -d $runningDistro -- bash -lc 'ps -eo pid=,args=' 2>$null)
        foreach ($processLine in $processLines) {
            if ($processLine -match '^\s*(\d+)\s+(.+)$') {
                $linuxPid = $Matches[1]
                $arguments = $Matches[2]
                if ($arguments -match 'qemu-system-riscv64' -and $arguments.Contains('build/wuwaos.img')) {
                    $cwd = & wsl.exe -d $runningDistro -- readlink "/proc/$linuxPid/cwd" 2>$null
                    if (($cwd -join '').Trim() -eq $wslRoot -or $arguments.Contains("$wslRoot/build/wuwaos.img")) {
                        throw "A QEMU process in WSL distro '$runningDistro' already uses this checkout's WUFS image (PID $linuxPid). Stop it before launching the desktop."
                    }
                }
            }
        }
    }

    # Ensure WSL can reach this checkout and has the required build/runtime tools before changing images.
    $dependencyOutput = & wsl.exe -d $Distro -- bash -lc 'command -v make >/dev/null && command -v clang >/dev/null && command -v ld.lld >/dev/null && command -v llvm-objcopy >/dev/null && command -v python3 >/dev/null && command -v qemu-system-riscv64 >/dev/null && test -f /usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin' 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "WSL dependencies are missing in '$Distro' (make, clang, ld.lld, llvm-objcopy, python3, qemu-system-riscv64, OpenSBI). $($dependencyOutput -join ' ')"
    }

    $vncDisplay = Select-FreePort 5901 5999
    $qmpPort = Select-FreePort 44000 49999
    New-Item -ItemType Directory -Path $sessionDir -Force | Out-Null
    if ($Snapshot) { $snapshotPathsOwned = $true }
    if ($Snapshot) {
        if (Test-Path -LiteralPath $baseDisk -PathType Leaf) { Copy-Item -LiteralPath $baseDisk -Destination $diskPath }
        if (Test-Path -LiteralPath $baseSwap -PathType Leaf) { Copy-Item -LiteralPath $baseSwap -Destination $swapPath }
    }
    if ($Snapshot) {
        $diskRelative = "build/desktop-$sessionId/wuwaos.img"
        $swapRelative = "build/desktop-$sessionId/wuwaos-swap.img"
    } else {
        $diskRelative = 'build/wuwaos.img'
        $swapRelative = 'build/wuwaos-swap.img'
    }
    $serialRelative = "build/desktop-$sessionId/serial.log"
    $pidFileRelative = "build/desktop-$sessionId/qemu.pid"
    $makeArgs = @(
        'run-gui-vnc-session',
        "DISK_IMAGE=$diskRelative",
        "SWAP_IMAGE=$swapRelative",
        "GUI_VNC_DISPLAY=$($vncDisplay - 5900)",
        "GUI_QMP_PORT=$qmpPort",
        "GUI_VM_NAME=$vmName",
        "GUI_SERIAL=file:$serialRelative",
        "GUI_PIDFILE=$pidFileRelative"
    ) | ForEach-Object { Escape-Bash $_ }
    $runner = "#!/usr/bin/env bash`nset -e`ncd $(Escape-Bash $wslRoot)`nexec make $($makeArgs -join ' ')`n"
    [IO.File]::WriteAllText($runnerPath, $runner, [Text.UTF8Encoding]::new($false))
    $buildArgs = @(
        'disk-image',
        "DISK_IMAGE=$diskRelative",
        "SWAP_IMAGE=$swapRelative"
    ) | ForEach-Object { Escape-Bash $_ }
    $buildDoneRelative = "build/desktop-$sessionId/build.done"
    $buildRunner = "#!/usr/bin/env bash`nset -e`ncd $(Escape-Bash $wslRoot)`nmake $($buildArgs -join ' ')`nprintf done > $(Escape-Bash $buildDoneRelative)`n"
    [IO.File]::WriteAllText($buildRunnerPath, $buildRunner, [Text.UTF8Encoding]::new($false))
    $stopRunner = @'
#!/usr/bin/env bash
set -e
pid_file=$1
expected_name=$2
runner_marker=$3
pid=''
if [[ -r "$pid_file" ]]; then
    candidate=$(cat "$pid_file")
    if [[ "$candidate" =~ ^[0-9]+$ && -r "/proc/$candidate/cmdline" ]]; then
        cmdline=$(tr '\0' ' ' < "/proc/$candidate/cmdline")
        if [[ "$cmdline" == *"qemu-system-riscv64"*"-name $expected_name"* ]]; then pid=$candidate; fi
    fi
fi
if [[ -z "$pid" ]]; then
    for cmd_path in /proc/[0-9]*/cmdline; do
        [[ -r "$cmd_path" ]] || continue
        candidate=${cmd_path#/proc/}
        candidate=${candidate%/cmdline}
        cmdline=$(tr '\0' ' ' < "$cmd_path")
        if [[ "$cmdline" == *"qemu-system-riscv64"*"-name $expected_name"* ]]; then pid=$candidate; break; fi
    done
fi
if [[ -z "$pid" ]]; then
    for cmd_path in /proc/[0-9]*/cmdline; do
        [[ -r "$cmd_path" ]] || continue
        candidate=${cmd_path#/proc/}
        candidate=${candidate%/cmdline}
        cmdline=$(tr '\0' ' ' < "$cmd_path")
        if [[ "$cmdline" == *"make"*"GUI_VM_NAME=$expected_name"* ]]; then pid=$candidate; break; fi
    done
fi
if [[ -z "$pid" ]]; then
    for cmd_path in /proc/[0-9]*/cmdline; do
        [[ -r "$cmd_path" ]] || continue
        candidate=${cmd_path#/proc/}
        candidate=${candidate%/cmdline}
        cmdline=$(tr '\0' ' ' < "$cmd_path")
        if [[ "$cmdline" == *"$runner_marker/run.sh"* ]]; then pid=$candidate; break; fi
    done
fi
if [[ -z "$pid" ]]; then exit 0; fi
kill "$pid"
for _ in {1..50}; do [[ ! -e "/proc/$pid" ]] && exit 0; sleep 0.2; done
echo "Owned launcher/QEMU pid $pid did not stop after SIGTERM" >&2
exit 3
'@
    $stopRunner = $stopRunner -replace "`r`n", "`n"
    [IO.File]::WriteAllText($stopPath, $stopRunner, [Text.UTF8Encoding]::new($false))
    $runnerWsl = Convert-ToWslPath $runnerPath
    $buildRunnerWsl = Convert-ToWslPath $buildRunnerPath
    $stopWsl = Convert-ToWslPath $stopPath
    $buildStdoutLog = Join-Path $sessionDir 'build.stdout.log'
    $buildStderrLog = Join-Path $sessionDir 'build.stderr.log'
    $stdoutLog = Join-Path $sessionDir 'qemu.stdout.log'
    $stderrLog = Join-Path $sessionDir 'qemu.stderr.log'

    Write-Host 'Building WUWAOS...'
    $buildArguments = "-d $Distro --exec bash $(Quote-WindowsArgument $buildRunnerWsl)"
    $buildProcess = Start-Process -FilePath $wsl.Source -ArgumentList $buildArguments -WindowStyle Hidden -RedirectStandardOutput $buildStdoutLog -RedirectStandardError $buildStderrLog -PassThru
    while (-not $buildProcess.HasExited) { Start-Sleep -Milliseconds 250; $buildProcess.Refresh() }
    $buildProcess.WaitForExit()
    $buildProcess.Refresh()
    if (-not (Test-Path -LiteralPath (Join-Path $sessionDir 'build.done') -PathType Leaf)) {
        $buildErrorTail = if (Test-Path $buildStderrLog) { (Get-Content $buildStderrLog -Tail 25) -join "`n" } else { '' }
        throw "Build process exited without a success marker (exit code $($buildProcess.ExitCode)). Logs: $sessionDir`n$buildErrorTail"
    }

    Write-Host 'Starting the background QEMU session...'
    $wslArguments = "-d $Distro --exec bash $(Quote-WindowsArgument $runnerWsl)"
    $qemuProcess = Start-Process -FilePath $wsl.Source -ArgumentList $wslArguments -WindowStyle Hidden -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog -PassThru
    $qemuStopped = $false

    $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($qemuProcess.HasExited) {
            $qemuProcess.WaitForExit()
            $qemuProcess.Refresh()
            $stderrTail = if (Test-Path $stderrLog) { (Get-Content $stderrLog -Tail 25) -join "`n" } else { '' }
            throw "Build or QEMU startup exited with code $($qemuProcess.ExitCode). Logs: $sessionDir`n$stderrTail"
        }
        try {
            $qmpClient = [Net.Sockets.TcpClient]::new()
            $qmpClient.ReceiveTimeout = 2000
            $qmpClient.SendTimeout = 2000
            $qmpClient.Connect([Net.IPAddress]::Loopback, $qmpPort)
            $qmpStream = $qmpClient.GetStream()
            $qmpStream.ReadTimeout = 2000
            $qmpReader = [IO.StreamReader]::new($qmpStream, [Text.Encoding]::UTF8)
            $greeting = $qmpReader.ReadLine() | ConvertFrom-Json
            if (-not $greeting.QMP) { throw 'Unexpected QMP greeting.' }
            $null = Send-Qmp 'qmp_capabilities'
            $nameResponse = Send-Qmp 'query-name'
            if ($nameResponse.return.name -ne $vmName) { throw 'QMP endpoint belongs to a different QEMU instance.' }
            $ownsQemu = $true
            break
        } catch {
            if ($qmpClient) { $qmpClient.Dispose(); $qmpClient = $null }
            Start-Sleep -Milliseconds 400
        }
    }
    if (-not $ownsQemu) { throw "QEMU did not become ready within $StartupTimeoutSeconds seconds. Logs: $sessionDir" }

    $viewerAddress = "127.0.0.1::$vncDisplay"
    $viewerStdout = Join-Path $sessionDir 'viewer.stdout.log'
    $viewerStderr = Join-Path $sessionDir 'viewer.stderr.log'
    [IO.File]::WriteAllText((Join-Path $sessionDir 'viewer.args.txt'), "$viewer`r`n$viewerAddress`r`n", [Text.UTF8Encoding]::new($false))
    $viewerProcess = Start-Process -FilePath $viewer -ArgumentList @($viewerAddress) -RedirectStandardOutput $viewerStdout -RedirectStandardError $viewerStderr -PassThru
    $viewerConnected = $false
    $viewerDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    $vncState = $null
    while ([DateTime]::UtcNow -lt $viewerDeadline) {
        $vncState = Send-Qmp 'query-vnc'
        if (@($vncState.return.clients).Count -gt 0) { $viewerConnected = $true; break }
        $viewerProcess.Refresh()
        if ($viewerProcess.HasExited) { break }
        Start-Sleep -Milliseconds 250
    }
    $vncService = $vncState.return.service
    [IO.File]::WriteAllText((Join-Path $sessionDir 'viewer-qmp.json'), ($vncState | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    $viewerProcess.Refresh()
    if (-not $viewerConnected -and $viewerProcess.HasExited) {
        $viewerError = if (Test-Path $viewerStderr) { (Get-Content $viewerStderr -Tail 15) -join "`n" } else { '' }
        throw "VNC viewer '$viewer' exited with code $($viewerProcess.ExitCode) before connecting to '$viewerAddress' (QEMU VNC service $vncService). Logs: $sessionDir`n$viewerError"
    }
    if (-not $viewerConnected) {
        throw "VNC viewer '$viewer' did not connect within $StartupTimeoutSeconds seconds to '$viewerAddress' (QEMU VNC service $vncService). Logs: $sessionDir"
    }
    if ($viewerConnected) {
        Write-Host "Desktop opened with '$viewer' at '$viewerAddress' (QEMU service $vncService; VNC client connected). Close the viewer window to stop this QEMU session. Logs: $sessionDir"
    } else {
        Write-Host "Viewer '$viewer' exited with code $($viewerProcess.ExitCode) before a VNC client connected to '$viewerAddress' (QEMU service $vncService); shutting down QEMU. Logs: $sessionDir"
    }

    while ($true) {
        Start-Sleep -Milliseconds 400
        $qemuProcess.Refresh()
        $viewerProcess.Refresh()
        if ($qemuProcess.HasExited -or $viewerProcess.HasExited) { break }
    }
    if ($qemuProcess.HasExited) { Write-Host 'QEMU stopped; closing the desktop viewer.' }
    else { Write-Host "Desktop viewer closed with exit code $($viewerProcess.ExitCode); shutting down QEMU." }
} catch {
    Write-Error $_
} finally {
    if ($buildProcess) {
        $buildProcess.Refresh()
        if (-not $buildProcess.HasExited) {
            Write-Host 'Waiting for the active disk build to finish before releasing the image lock.'
            while (-not $buildProcess.HasExited) { Start-Sleep -Milliseconds 250; $buildProcess.Refresh() }
        }
    }
    if ($ownsQemu -and $qmpClient -and $qmpClient.Connected) {
        try {
            $null = Send-Qmp 'quit'
            $stopDeadline = [DateTime]::UtcNow.AddSeconds(10)
            while ([DateTime]::UtcNow -lt $stopDeadline) {
                $qemuProcess.Refresh()
                if ($qemuProcess.HasExited) { break }
                Start-Sleep -Milliseconds 200
            }
            if (-not $qemuProcess.HasExited) {
                Write-Warning "QEMU did not exit after QMP quit within 10 seconds; leaving its process untouched. Logs: $sessionDir"
            }
        } catch {
            Write-Warning "Could not send QMP quit: $_. The launcher will not terminate an unidentified process."
        }
    }
    if ($qmpClient) { $qmpClient.Dispose() }
    if ($qemuProcess -and $stopWsl) {
        $pidFileWsl = Convert-ToWslPath $pidFile
        $stopOutput = & wsl.exe -d $Distro --exec bash $stopWsl $pidFileWsl $vmName "desktop-$sessionId" 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Warning "Could not confirm and stop the owned Linux launcher/QEMU process: $($stopOutput -join ' ')" }
        else { $qemuStopped = $true }
    } elseif ($qemuProcess -and $qemuProcess.HasExited) {
        $qemuStopped = $true
    }
    if ($qemuProcess) {
        $stopDeadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $qemuProcess.Refresh()
            if ($qemuProcess.HasExited) { break }
            Start-Sleep -Milliseconds 200
        } while ([DateTime]::UtcNow -lt $stopDeadline)
        if (-not $qemuProcess.HasExited -and $qemuStopped) {
            Stop-Process -Id $qemuProcess.Id -ErrorAction SilentlyContinue
            Start-Sleep -Milliseconds 500
            $qemuProcess.Refresh()
        }
        if (-not $qemuProcess.HasExited) { Write-Warning "WSL launcher has not exited. It was left untouched. Logs: $sessionDir" }
    }
    if ($vncDisplay -gt 0 -and $qmpPort -gt 0) {
        $portDeadline = [DateTime]::UtcNow.AddSeconds(5)
        while ([DateTime]::UtcNow -lt $portDeadline) {
            if ((Test-PortFree $vncDisplay) -and (Test-PortFree $qmpPort)) { break }
            Start-Sleep -Milliseconds 200
        }
        if (-not (Test-PortFree $vncDisplay) -or -not (Test-PortFree $qmpPort)) {
            Write-Warning "VNC or QMP port did not release within 5 seconds."
        }
    }
    if ($viewerProcess -and -not $viewerProcess.HasExited) {
        try { Stop-Process -Id $viewerProcess.Id -ErrorAction SilentlyContinue } catch { }
    }
    if ($Snapshot -and $snapshotPathsOwned) {
        $sessionRoot = [IO.Path]::GetFullPath($sessionDir).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        $safeSnapshotPaths = @($snapshotDiskPath, $snapshotSwapPath) | ForEach-Object { [IO.Path]::GetFullPath($_) }
        $pathsOutsideSession = @($safeSnapshotPaths | Where-Object { -not $_.StartsWith($sessionRoot, [StringComparison]::OrdinalIgnoreCase) })
        if ($pathsOutsideSession.Count -gt 0 -or $safeSnapshotPaths -contains [IO.Path]::GetFullPath($baseDisk) -or $safeSnapshotPaths -contains [IO.Path]::GetFullPath($baseSwap)) {
            Write-Warning 'Snapshot cleanup path validation failed; temporary images were left in place.'
        } elseif ($qemuStopped) {
            Remove-Item -LiteralPath $safeSnapshotPaths -Force -ErrorAction SilentlyContinue
        } else {
            Write-Warning "Snapshot images were retained because QEMU shutdown was not confirmed: $sessionDir"
        }
    }
    if ($lockStream) { $lockStream.Dispose() }
}
