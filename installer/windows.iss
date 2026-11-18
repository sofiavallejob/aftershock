; Inno Setup script for Aftershock (built by the GitHub workflow)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6F3B2E9A-4C1D-4E7B-9A2F-AF7E2B1C0D55}
AppName=Aftershock
AppVersion={#AppVersion}
AppPublisher=Sofia Vallejo Budziszewski
AppPublisherURL=https://sofiavallejob.github.io/sofiavallejo/
AppSupportURL=https://github.com/sofiavallejob/aftershock
DefaultDirName={autopf}\Aftershock
DefaultGroupName=Aftershock
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputBaseFilename=Aftershock-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Aftershock

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Reaper, Ableton Live, Cubase, Bitwig, FL Studio...)"; Types: full custom
Name: "app"; Description: "Standalone app"; Types: full custom

[Files]
Source: "{#BuildDir}\VST3\Aftershock.vst3\*"; DestDir: "{commoncf64}\VST3\Aftershock.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#BuildDir}\Standalone\Aftershock.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{group}\Aftershock"; Filename: "{app}\Aftershock.exe"; Components: app
Name: "{group}\Uninstall Aftershock"; Filename: "{uninstallexe}"
