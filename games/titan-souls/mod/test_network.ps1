# Titan Souls Dual-Instance Network Test Runner
# Launches two local instances (Host on 7777, Client on 7778) to verify network co-op.
param (
    [string]$GameDir = "."
)

if (-not (Test-Path "$GameDir\TITAN.exe")) {
    Write-Host "TITAN.exe not found in $GameDir. Specify -GameDir pointing to your Titan Souls install folder."
    Write-Host "Example: .\test_network.ps1 -GameDir `"C:\Games\TitanSouls`""
    exit 1
}

if (-not (Test-Path "bin\version.dll")) {
    Write-Error "bin\version.dll not found! Run .\build.bat before running the test."
    exit 1
}

Write-Host ">>> Titan Souls Network Test Runner"
Write-Host "Copying version.dll and host/client configs to isolated run directories..."
$hostDir = "$env:TEMP\TitanSouls_Host"
$clientDir = "$env:TEMP\TitanSouls_Client"

New-Item -ItemType Directory -Force -Path $hostDir | Out-Null
New-Item -ItemType Directory -Force -Path $clientDir | Out-Null

Copy-Item "$GameDir\*" -Destination $hostDir -Recurse -Force
Copy-Item "$GameDir\*" -Destination $clientDir -Recurse -Force

Copy-Item "bin\version.dll" "$hostDir\version.dll" -Force
Copy-Item "bin\version.dll" "$clientDir\version.dll" -Force

# Configure Host
@"
[General]
QuickStart=1
RunAutomatedTest=0
MidpointCamera=1

[Network]
Enabled=1
Role=1
BindPort=7777
RemoteIP=127.0.0.1
RemotePort=7778
IndependentCamera=0

[Input]
GamepadIndex=0
Keyboard2Enabled=0

[Player2]
ColorR=0.2
ColorG=0.8
ColorB=1.0
ColorA=1.0
"@ | Set-Content "$hostDir\coop_config.ini"

# Configure Client
@"
[General]
QuickStart=1
RunAutomatedTest=0
MidpointCamera=1

[Network]
Enabled=1
Role=2
BindPort=7778
RemoteIP=127.0.0.1
RemotePort=7777
IndependentCamera=1

[Input]
GamepadIndex=0
Keyboard2Enabled=1

[Player2]
ColorR=1.0
ColorG=0.5
ColorB=0.2
ColorA=1.0
"@ | Set-Content "$clientDir\coop_config.ini"

Write-Host "Starting Host instance on port 7777..."
$procHost = Start-Process -FilePath "$hostDir\TITAN.exe" -WorkingDirectory $hostDir -PassThru
Start-Sleep -Seconds 2

Write-Host "Starting Client instance on port 7778..."
$procClient = Start-Process -FilePath "$clientDir\TITAN.exe" -WorkingDirectory $clientDir -PassThru

Write-Host "Both instances running! Close both windows when finished testing."
