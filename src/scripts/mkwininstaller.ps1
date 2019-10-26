param (
    [Parameter(Mandatory=$true)][string]$version
)

$target="silentdragon-v$version"

Remove-Item -Path release/wininstaller -Recurse -ErrorAction Ignore  | Out-Null
New-Item release/wininstaller -itemtype directory                    | Out-Null

Copy-Item release/$target/silentdragon.exe     release/wininstaller/
Copy-Item release/$target/LICENSE           release/wininstaller/
Copy-Item release/$target/README.md         release/wininstaller/
Copy-Item release/$target/hushd.exe        release/wininstaller/
Copy-Item release/$target/hush-cli.exe     release/wininstaller/

Get-Content src/scripts/silentdragonlite.wxs | ForEach-Object { $_ -replace "RELEASE_VERSION", "$version" } | Out-File -Encoding utf8 release/wininstaller/silentdragonlite.wxs

candle.exe release/wininstaller/silentdragonlite.wxs -o release/wininstaller/silentdragonlite.wixobj 
if (!$?) {
    exit 1;
}

light.exe -ext WixUIExtension -cultures:en-us release/wininstaller/silentdragonlite.wixobj -out release/wininstaller/silentdragon.msi 
if (!$?) {
    exit 1;
}

New-Item artifacts -itemtype directory -Force | Out-Null
Copy-Item release/wininstaller/silentdragon.msi ./artifacts/Windows-installer-$target.msi