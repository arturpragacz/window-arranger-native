
$registryPath = "HKCU:\Software\Mozilla\NativeMessagingHosts"

$name = "window_arranger"

$value = $PSScriptRoot + "\window_arranger.json"

New-Item -Path $registryPath -Name $name -Value $value -Force
