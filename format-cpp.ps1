# PowerShell script to run clang-format on all C++ files in Source and Plugins\USFLoader directories

Write-Host "Starting C++ file formatting..." -ForegroundColor Green

# Define directories to search
$directories = @(
    "Source",
    "Plugins\USFLoader"
)

# C++ file extensions to format
$extensions = @("*.cpp", "*.h", "*.hpp", "*.cc", "*.cxx")

$totalFiles = 0
$formattedFiles = 0

foreach ($directory in $directories) {
    if (Test-Path $directory) {
        Write-Host "Processing directory: $directory" -ForegroundColor Yellow

        foreach ($extension in $extensions) {
            $files = Get-ChildItem -Path $directory -Filter $extension -Recurse -File

            foreach ($file in $files) {
                $totalFiles++
                Write-Host "Formatting: $($file.FullName)" -ForegroundColor Cyan

                try {
                    & clang-format -i $file.FullName
                    $formattedFiles++
                } catch {
                    Write-Host "Error formatting $($file.FullName): $($_.Exception.Message)" -ForegroundColor Red
                }
            }
        }
    } else {
        Write-Host "Directory not found: $directory" -ForegroundColor Red
    }
}

Write-Host "`nFormatting complete!" -ForegroundColor Green
Write-Host "Total files processed: $totalFiles" -ForegroundColor White
Write-Host "Successfully formatted: $formattedFiles" -ForegroundColor White

if ($formattedFiles -lt $totalFiles) {
    Write-Host "Some files had formatting errors - see output above" -ForegroundColor Yellow
}