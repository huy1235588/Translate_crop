$EXE_PATH = "e:\Project\Cpp\Translate_crop\build\TranslateCrop.exe"
$OUTPUT_DIR = "e:\Project\Cpp\Translate_crop\dist"
$MSYS2_BIN = "D:\Program Files\MSYS2\mingw64\bin"

# 1. Tạo thư mục output
New-Item -ItemType Directory -Force -Path $OUTPUT_DIR | Out-Null

# 2. Copy exe
Copy-Item -Path $EXE_PATH -Destination $OUTPUT_DIR -Force

# 3. Tìm tất cả phụ thuộc DLL (đệ quy)
$allDeps = @()
$queue = @($EXE_PATH)

while ($queue.Count -gt 0) {
    $current = $queue[0]
    $queue = $queue[1..$queue.Length]
    
    $deps = & "$MSYS2_BIN\ntldd.exe" -R $current | 
            Where-Object { $_ -match "=>.*\.dll" -and 
                          -not $_.Contains("C:\Windows") -and
                          -not $_.Contains("api-ms-win-") } |
            ForEach-Object { $_.Split("=>")[1].Trim() }
    
    foreach ($dep in $deps) {
        if (-not $allDeps.Contains($dep)) {
            $allDeps += $dep
            $queue += $dep
        }
    }
}

# 4. Copy tất cả DLL phụ thuộc
$allDeps | ForEach-Object {
    if (Test-Path $_) {
        $dllName = Split-Path $_ -Leaf
        $dest = Join-Path $OUTPUT_DIR $dllName
        if (-not (Test-Path $dest)) {
            Copy-Item $_ $dest -Force
            Write-Host "Copied: $dllName"
        }
    }
}

# 5. Verify
Write-Host "Verifying dependencies..."
& "$MSYS2_BIN\ntldd.exe" -R (Join-Path $OUTPUT_DIR (Split-Path $EXE_PATH -Leaf))