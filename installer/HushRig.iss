; Instalador do HushRig (Inno Setup 6). Compilado pelo CI, com estes defines:
;   /DMyAppVersion=0.1.0
;   /DBuildDir=<pasta build\HushRig_artefacts\Release>
;   /DFlexAsioInstaller=<caminho do instalador do FlexASIO>

#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef BuildDir
  #error BuildDir nao definido
#endif
#ifndef FlexAsioInstaller
  #error FlexAsioInstaller nao definido
#endif

[Setup]
AppId={{8F3C2A51-6B7D-4E1A-9C42-5D0E7A1B3F68}
AppName=HushRig
AppVersion={#MyAppVersion}
AppPublisher=HushRig
DefaultDirName={autopf}\HushRig
DefaultGroupName=HushRig
OutputDir=..\dist
OutputBaseFilename=HushRig-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=..\LICENSE
UninstallDisplayName=HushRig
SetupIconFile=..\assets\hushrig.ico
UninstallDisplayIcon={app}\HushRig.exe

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Types]
Name: "full"; Description: "Instalacao completa"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "app"; Description: "HushRig (aplicativo)"; Types: full custom; Flags: fixed
Name: "vst3"; Description: "Plugin VST3 (para usar em DAWs)"; Types: full
Name: "flexasio"; Description: "FlexASIO (driver ASIO de baixa latencia)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na area de trabalho"; Components: app

[Files]
Source: "{#BuildDir}\Standalone\HushRig.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#BuildDir}\VST3\HushRig.vst3\*"; DestDir: "{commonpf}\Common Files\VST3\HushRig.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#FlexAsioInstaller}"; DestDir: "{tmp}"; DestName: "FlexASIO-setup.exe"; Components: flexasio; Flags: deleteafterinstall

[Icons]
Name: "{group}\HushRig"; Filename: "{app}\HushRig.exe"; Components: app
Name: "{group}\Desinstalar HushRig"; Filename: "{uninstallexe}"
Name: "{autodesktop}\HushRig"; Filename: "{app}\HushRig.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\FlexASIO-setup.exe"; Parameters: "/S"; StatusMsg: "Instalando o FlexASIO..."; Components: flexasio; Flags: waituntilterminated
Filename: "{app}\HushRig.exe"; Description: "Abrir o HushRig"; Flags: nowait postinstall skipifsilent
; Atualização automática pelo app (instalação silenciosa): reabre o HushRig ao terminar
Filename: "{app}\HushRig.exe"; Flags: nowait; Check: WizardSilent
