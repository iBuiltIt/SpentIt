; Per-user SpentIt installer. tools/package.py writes setup-defines.iss and compiles this script.
; AppId is uuid5(DNS, "com.stefanwessels.SpentIt"). Do not change it: upgrades replace this install.
#ifndef SourceDir
  #error Compile this script through tools/package.py so SourceDir is defined.
#endif

[Setup]
AppId={{6530116C-C0C6-5439-8698-8BB345D8ABE8}}
AppName=SpentIt
AppVersion={#AppVersion}
AppPublisher=Stefan Wessels
AppCopyright=(c) 2026 Stefan Wessels
VersionInfoVersion={#VersionInfo}
VersionInfoProductName=SpentIt
VersionInfoDescription=SpentIt
DefaultDirName={localappdata}\Programs\SpentIt
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename={#SetupBaseName}
SetupIconFile={#SetupIcon}
UninstallDisplayIcon={app}\SpentIt.exe
UninstallDisplayName=SpentIt
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes
CloseApplicationsFilter=SpentIt.exe
RestartApplications=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
#ifdef SignSetup
SignTool=spentit
SignedUninstaller=yes
#endif

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\SpentIt"; Filename: "{app}\SpentIt.exe"
Name: "{autodesktop}\SpentIt"; Filename: "{app}\SpentIt.exe"

[Registry]
Root: HKCU; Subkey: "Software\Classes\.spentit"; ValueType: string; ValueName: ""; ValueData: "SpentIt.Document"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\.spentit\OpenWithProgids"; ValueType: string; ValueName: "SpentIt.Document"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\SpentIt.Document"; ValueType: string; ValueName: ""; ValueData: "SpentIt file"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SpentIt.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\SpentIt.exe,0"
Root: HKCU; Subkey: "Software\Classes\SpentIt.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\SpentIt.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\SpentIt.exe"; ValueType: string; ValueName: ""; ValueData: "SpentIt"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\SpentIt.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\SpentIt.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\SpentIt.exe\SupportedTypes"; ValueType: string; ValueName: ".spentit"; ValueData: ""

[Run]
Filename: "{app}\SpentIt.exe"; Description: "Open SpentIt"; Flags: postinstall nowait skipifsilent
