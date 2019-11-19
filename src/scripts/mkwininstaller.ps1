param (
    [Parameter(Mandatory=$true)][string]$version
)

$target="SilentDragonLite-v$version"

Remove-Item -Path release/wininstaller -Recurse -ErrorAction Ignore  | Out-Null
New-Item release/wininstaller -itemtype directory                    | Out-Null

Copy-Item release/$target/SilentDragonLite.exe     release/wininstaller/
Copy-Item release/$target/LICENSE                  release/wininstaller/

Get-Content src/scripts/silentdragonlite.wxs | ForEach-Object { $_ -replace "RELEASE_VERSION", "$version" } | Out-File -Encoding utf8 release/wininstaller/silentdragonlite.wxs

candle.exe release/wininstaller/silentdragonlite.wxs -o release/wininstaller/silentdragonlite.wixobj 
if (!$?) {
    exit 1;
}

light.exe -ext WixUIExtension -cultures:en-us release/wininstaller/SilentDragonLite.wixobj -out release/wininstaller/SilentDragonLite.msi 
if (!$?) {
    exit 1;
}

New-Item artifacts -itemtype directory -Force | Out-Null
Copy-Item release/wininstaller/SilentDragonLite.msi ./artifacts/Windows-installer-$target.msi
