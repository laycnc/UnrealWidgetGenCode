import os
import subprocess
import BuildHelper

ProjectFilePath = BuildHelper.GetProjectFilePath()

if ProjectFilePath == None:
	os.abort()

InstalledDirectory = BuildHelper.GetEngineDir(ProjectFilePath)

if InstalledDirectory == None:
	os.abort()
pass

UnrealEditorCmd: str = f"{InstalledDirectory}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
subprocess.run(f"{UnrealEditorCmd} {ProjectFilePath} -run=GatherText -config=Plugins/WidgetGenCodeTool/Config/Localization/WidgetGenCodeTool.ini")
