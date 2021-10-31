# Window Arranger

This application serves as the Native Application for the [Window Arranger extension](https://github.com/pragacz/window-arranger-webext).

## Installation

This application is available on the Windows operating system only.

1. Download the latest version of the application from the [releases](https://github.com/pragacz/window-arranger-native/releases/).
2. Unpack the zip archive.
3. Run the `register_native.ps1` Powershell script. If you've never run Powershell scripts before, you may need to [set a correct execution policy](https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.security/set-executionpolicy?view=powershell-7#example-1--set-an-execution-policy), for example by running a Powershell command:
	
	`Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine`

## Usage

If you configured everything correctly, the Native Application should be started automatically by the Webextension.

## Dependencies

- [Magic Enum](https://github.com/Neargye/magic_enum)
- [rapidjson](https://rapidjson.org/)
- [7+ Taskbar Tweaking Library](https://rammichael.com/7-taskbar-tweaking-library)
