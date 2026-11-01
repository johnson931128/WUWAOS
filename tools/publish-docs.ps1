param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$distPath = Join-Path $repoRoot 'docs/.vitepress/dist'
$indexPath = Join-Path $repoRoot ('build/docs-publish-' + [guid]::NewGuid().ToString('N') + '.index')
$previousBase = $env:DOCS_BASE
$previousIndex = $env:GIT_INDEX_FILE
$previousLocation = Get-Location

function Invoke-DocsGit {
    param([string[]]$GitArguments)
    $result = & git -C $repoRoot @GitArguments
    if ($LASTEXITCODE -ne 0) { throw "Git failed: $($GitArguments[0])" }
    return $result
}

try {
    Set-Location $repoRoot
    $remote = Invoke-DocsGit @('remote', 'get-url', 'origin')
    if ($remote -notin @('git@github.com:johnson931128/WUWAOS.git', 'https://github.com/johnson931128/WUWAOS.git')) {
        throw 'Refusing to publish to an unexpected origin.'
    }
    $env:DOCS_BASE = '/WUWAOS/'
    & npm.cmd run docs:build
    if ($LASTEXITCODE -ne 0) { throw 'Documentation build failed.' }
    if (!(Test-Path -LiteralPath (Join-Path $distPath 'index.html'))) { throw 'Missing built homepage.' }
    [System.IO.File]::WriteAllText((Join-Path $distPath '.nojekyll'), '')
    New-Item -ItemType Directory -Path (Join-Path $repoRoot 'build') -Force | Out-Null

    # Record only generated static output. No source branch writes, checkout, or force push.
    $remoteBranch = Invoke-DocsGit @('ls-remote', '--heads', 'origin', 'refs/heads/gh-pages')
    $parentArguments = @()
    if ($remoteBranch) {
        Invoke-DocsGit @('fetch', 'origin', 'refs/heads/gh-pages') | Out-Null
        $parent = Invoke-DocsGit @('rev-parse', 'FETCH_HEAD')
        $parentArguments = @('-p', $parent)
    }
    $env:GIT_INDEX_FILE = $indexPath
    Invoke-DocsGit @('read-tree', '--empty') | Out-Null
    Invoke-DocsGit @('-C', $distPath, '--work-tree', $distPath, 'add', '--all', '--', '.') | Out-Null
    $tree = Invoke-DocsGit @('write-tree')
    $commit = Invoke-DocsGit (@('commit-tree', $tree) + $parentArguments + @('-m', 'Publish WUWAOS documentation website'))
    # No force push: a concurrent publication must be reviewed and rebuilt.
    Invoke-DocsGit @('push', 'origin', "${commit}:refs/heads/gh-pages")
    Write-Output "Published static commit: $commit"
    Write-Output 'GitHub Pages URL: https://johnson931128.github.io/WUWAOS/'
}
finally {
    $env:DOCS_BASE = $previousBase
    $env:GIT_INDEX_FILE = $previousIndex
    Set-Location $previousLocation
    if (Test-Path -LiteralPath $indexPath) { Remove-Item -LiteralPath $indexPath }
}
