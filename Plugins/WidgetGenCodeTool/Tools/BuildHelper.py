import os
import json
from typing import Optional, TypedDict, List
import winreg

class UnrealModule(TypedDict):
	Name: str
	Type: str
	LoadingPhase: str
	AdditionalDependencies: List[str]
pass

class UnrealPlugins(TypedDict):
	Name: str
	Enabled: bool
	TargetAllowList: List[str]
pass

class UnrealProject(TypedDict):
	FileVersion: int
	EngineAssociation: str
	Category: str
	Description: str
	Modules: List[UnrealModule]
	Plugins: List[UnrealPlugins]
pass


def GetProjectFilePath() -> Optional[str]:
	# 現在位置からプロジェクトファイルが存在するディレクトリを取得する
	ProjectDir = os.path.normpath( f"{os.path.dirname(__file__)}\\..\\..\\..")

	for item in os.listdir(ProjectDir):
		if ".uproject" in item:
			return os.path.join(ProjectDir, item)
		pass
	pass
	return None
pass

def GetEngineDir(InPrpjectDir: str) -> Optional[str]:

	with open(InPrpjectDir) as file:

		# uprojectファイルからエンジンのバージョンを取得する
		UnrealProjectFile: Optional[UnrealProject]  = json.load(file)

		if UnrealProjectFile == None:
			return None
		pass

		EngineAssociation = UnrealProjectFile["EngineAssociation"]

		# レジストリにエンジンのパスが登録されているので其方を参照する
		UnrealEngineKey =  f"SOFTWARE\\EpicGames\\Unreal Engine\\{EngineAssociation}"
		key = winreg.OpenKeyEx(winreg.HKEY_LOCAL_MACHINE, UnrealEngineKey, winreg.KEY_READ)
		InstalledDirectory = winreg.QueryValueEx(key, "InstalledDirectory")

		return InstalledDirectory[0]

	return None
pass
