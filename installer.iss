; ── Wave Installer (Inno Setup) ────────────────────────────────
; Build with: iscc installer.iss
; Requires: Inno Setup 6+ (https://jrsoftware.org/isinfo.php)
;
; Before running: execute package.bat Release to create dist\Wave\

[Setup]
AppName=Wave Audio Player
AppVersion=0.1.0-beta
AppPublisher=Wave
AppPublisherURL=https://github.com/azmin/wave
DefaultDirName={autopf}\Wave
DefaultGroupName=Wave
OutputDir=dist
OutputBaseFilename=WaveSetup-0.1.0-beta
Compression=lzma2
SolidCompression=yes
SetupIconFile=assets\wave.ico
UninstallDisplayIcon={app}\Wave.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"
Name: "startmenu"; Description: "Create a &Start Menu shortcut"; GroupDescription: "Additional shortcuts:"; Flags: checkedonce

[Files]
Source: "dist\Wave\Wave.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\Wave\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\Wave\mpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\Wave\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "dist\Wave\sdk\*"; DestDir: "{app}\sdk"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\Wave"; Filename: "{app}\Wave.exe"; Tasks: startmenu
Name: "{group}\Uninstall Wave"; Filename: "{uninstallexe}"; Tasks: startmenu
Name: "{autodesktop}\Wave"; Filename: "{app}\Wave.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Wave.exe"; Description: "Launch Wave"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\plugins"
Type: filesandordirs; Name: "{app}\data"
