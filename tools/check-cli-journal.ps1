param([Parameter(Mandatory = $true)][string]$Image)

$ErrorActionPreference = 'Stop'
try {
    $disk = [IO.File]::Open($Image, 'Open', 'Read', 'Read')
    try {
        $reader = [IO.BinaryReader]::new($disk)
        $magic = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(8))
        if ($magic -cne 'WUFS0002' -and $magic -cne 'WUFS0003') { exit 0 }

        $disk.Position = 512
        $header = $reader.ReadBytes(512)
        if ($header.Length -ne 512) { throw 'The WUFS journal header is truncated.' }
        foreach ($byte in $header) {
            if ($byte -ne 0) { exit 10 }
        }
        exit 0
    } finally {
        $disk.Dispose()
    }
} catch {
    [Console]::Error.WriteLine("WUWAOS: Cannot check the boot disk. $($_.Exception.Message)")
    exit 1
}
