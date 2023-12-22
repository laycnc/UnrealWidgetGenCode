#include "WidgetGenCodeProjectUtils.h"

#include "Misc/Build.h"
#include "GameProjectUtils.h"
#include "ClassTemplateEditorSubsystem.h"
#include "Editor.h"
#include "BlueprintEditor.h"
#include "GeneralProjectSettings.h"
#include "Toolkits/ToolkitManager.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "SourceCodeNavigation.h"
#include "PluginBlueprintLibrary.h"
#include "Misc/ScopedSlowTask.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "Misc/HotReloadInterface.h"
#include "Editor/EditorPerProjectUserSettings.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

#define LOCTEXT_NAMESPACE "FWidgetGenCodeToolModule"

namespace
{
	constexpr static const TCHAR IncludePathFormatString[] = TEXT("#include \"%s\"");

	FString MakeCopyrightLine()
	{
		const FString CopyrightNotice = GetDefault<UGeneralProjectSettings>()->CopyrightNotice;
		if (!CopyrightNotice.IsEmpty())
		{
			return FString(TEXT("// ")) + CopyrightNotice;
		}
		else
		{
			return FString();
		}
	}

	void HarvestCursorSyncLocation(FString& FinalOutput, FString& OutSyncLocation)
	{
		OutSyncLocation.Empty();

		// Determine the cursor focus location if this file will by synced after creation
		TArray<FString> Lines;
		FinalOutput.ParseIntoArray(Lines, TEXT("\n"), false);
		for (int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx)
		{
			const FString& Line = Lines[LineIdx];
			int32 CharLoc = Line.Find(TEXT("%CURSORFOCUSLOCATION%"));
			if (CharLoc != INDEX_NONE)
			{
				// Found the sync marker
				OutSyncLocation = FString::Printf(TEXT("%d:%d"), LineIdx + 1, CharLoc + 1);
				break;
			}
		}

		// If we did not find the sync location, just sync to the top of the file
		if (OutSyncLocation.IsEmpty())
		{
			OutSyncLocation = TEXT("1:1");
		}

		// Now remove the cursor focus marker
		FinalOutput = FinalOutput.Replace(TEXT("%CURSORFOCUSLOCATION%"), TEXT(""), ESearchCase::CaseSensitive);
	}

	bool GenerateConstructorDeclaration(FString& Out, const FString& PrefixedClassName, FText& OutFailReason)
	{
		FString Template;
		if (!GameProjectUtils::ReadTemplateFile(TEXT("UObjectClassConstructorDeclaration.template"), Template, OutFailReason))
		{
			return false;
		}

		Out = Template.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);

		return true;
	}

	FString ReplaceWildcard(const FString& Input, const FString& From, const FString& To, bool bLeadingTab, bool bTrailingNewLine)
	{
		FString Result = Input;
		FString WildCard = bLeadingTab ? TEXT("\t") : TEXT("");

		WildCard.Append(From);

		if (bTrailingNewLine)
		{
			WildCard.Append(LINE_TERMINATOR);
		}

		int32 NumReplacements = Result.ReplaceInline(*WildCard, *To, ESearchCase::CaseSensitive);

		// if replacement fails, try again using just the plain wildcard without tab and/or new line
		if (NumReplacements == 0)
		{
			Result = Result.Replace(*From, *To, ESearchCase::CaseSensitive);
		}

		return Result;
	}


	bool ReadWidgetGenTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason)
	{
		FString ContentDir;
		if (UPluginBlueprintLibrary::GetPluginContentDir(TEXT("WidgetGenCodeTool"), ContentDir))
		{
			const FString FullFileName = ContentDir / TEXT("Editor") / TEXT("Templates") / TemplateFileName;

			if (FFileHelper::LoadFileToString(OutFileContents, *FullFileName))
			{
				return true;
			}

			FFormatNamedArguments Args;
			Args.Add(TEXT("FullFileName"), FText::FromString(FullFileName));
			OutFailReason = FText::Format(LOCTEXT("FailedToReadTemplateFile", "Failed to read template file \"{FullFileName}\""), Args);
		}

		return false;
	}

	bool GetClassIncludePath(const UClass* InClass, FString& OutIncludePath)
	{
		if (InClass && InClass->HasMetaData(TEXT("IncludePath")))
		{
			OutIncludePath = InClass->GetMetaData(TEXT("IncludePath"));
			return true;
		}
		return false;
	}


	static bool TryParseIncludeDirective(const FString& Text, int StartPos, int EndPos, FString& IncludePath)
	{
		// Check if the line starts with a # character
		int Pos = StartPos;
		while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
		{
			Pos++;
		}
		if (Pos == EndPos || Text[Pos++] != '#')
		{
			return false;
		}
		while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
		{
			Pos++;
		}

		// Check it's an include directive
		const TCHAR* IncludeText = TEXT("include");
		for (int Idx = 0; IncludeText[Idx] != 0; Idx++)
		{
			if (Pos == EndPos || Text[Pos] != IncludeText[Idx])
			{
				return false;
			}
			Pos++;
		}
		while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
		{
			Pos++;
		}

		// Parse out the quoted include path
		if (Pos == EndPos || Text[Pos++] != '"')
		{
			return false;
		}
		int IncludePathPos = Pos;
		while (Pos < EndPos && Text[Pos] != '"')
		{
			Pos++;
		}
		IncludePath = Text.Mid(IncludePathPos, Pos - IncludePathPos);
		return true;
	}

	static bool IsUsingOldStylePch(FString BaseDir)
	{
		// Find all the cpp files under the base directory
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *BaseDir, TEXT("*.cpp"), true, false, false);

		// Parse the first include directive for up to 16 include paths
		TArray<FString> FirstIncludedFiles;
		for (int Idx = 0; Idx < Files.Num() && Idx < 16; Idx++)
		{
			FString Text;
			FFileHelper::LoadFileToString(Text, *Files[Idx]);

			int LinePos = 0;
			while (LinePos < Text.Len())
			{
				int EndOfLinePos = LinePos;
				while (EndOfLinePos < Text.Len() && Text[EndOfLinePos] != '\n')
				{
					EndOfLinePos++;
				}

				FString IncludePath;
				if (TryParseIncludeDirective(Text, LinePos, EndOfLinePos, IncludePath))
				{
					FirstIncludedFiles.AddUnique(FPaths::GetCleanFilename(IncludePath));
					break;
				}

				LinePos = EndOfLinePos + 1;
			}
		}
		return FirstIncludedFiles.Num() == 1 && Files.Num() > 1;
	}


	FString GetIncludePathForFile(const FString& InFullFilePath, const FString& ModuleRootPath)
	{
		FString RelativeHeaderPath = FPaths::ChangeExtension(InFullFilePath, "h");

		const FString PublicString = ModuleRootPath / "Public" / "";
		if (RelativeHeaderPath.StartsWith(PublicString))
		{
			return RelativeHeaderPath.RightChop(PublicString.Len());
		}

		const FString PrivateString = ModuleRootPath / "Private" / "";
		if (RelativeHeaderPath.StartsWith(PrivateString))
		{
			return RelativeHeaderPath.RightChop(PrivateString.Len());
		}

		return RelativeHeaderPath.RightChop(ModuleRootPath.Len());
	}

}

bool WidgetGenCodeProjectUtils::GenWidgetWidgetInfo(UWidgetBlueprint* InBlueprint, FWidgetGenClassInfomation& OutBaseClassInfo, FWidgetGenClassInfomation& OutImplmentClassInfo)
{
	FString BlueprintClassName = InBlueprint->GeneratedClass->GetName();
	BlueprintClassName.RemoveFromEnd(TEXT("_C"));

	UClass* ParentClass = InBlueprint->ParentClass;

	// 一番新しいネイティブクラスが対象
	while (IsValid(ParentClass) && !ParentClass->IsNative())
	{
		ParentClass = ParentClass->GetSuperClass();
	}

	if (!IsValid(ParentClass))
	{
		return false;
	}

	// 生成コードをすでに親クラスに設定している
	if (ParentClass->HasMetaData(TEXT("WidgetGen")))
	{
		// 実装されたサンプルのコード例
	//　class BP_Menu : public UWidgetGenImplBP_Menu
	// 自動生成したランタイム実装用クラス
	// UCLASS(meta = (WidgetGenImpl, WidgetGen = "/Game/UI/BP_Menu"))
	//　class UWidgetGenImplBP_Menu : public UWidgetGenBaseBP_Menu
	// 自動生成したランタイム実装用クラス
	// UCLASS(meta = (WidgetGenBase, WidgetGen = "/Game/UI/BP_Menu"))
	//　class UWidgetGenBaseBP_Menu : public UUserWidget

		UClass* WidgetGenImplClass = ParentClass;
		UClass* WidgetGenBaseClass = ParentClass->GetSuperClass();


		const auto FindModule = [](const FString& InModuleName) -> TOptional<FModuleContextInfo>
			{
				for (const FModuleContextInfo& ModuleInfo : GameProjectUtils::GetCurrentProjectModules())
				{
					if (ModuleInfo.ModuleName == InModuleName)
					{
						return ModuleInfo;
					}
				}
				for (const FModuleContextInfo& ModuleInfo : GameProjectUtils::GetCurrentProjectPluginModules())
				{
					if (ModuleInfo.ModuleName == InModuleName)
					{
						return ModuleInfo;
					}
				}
				return NullOpt;
			};

		if (WidgetGenImplClass->HasMetaData(TEXT("WidgetGenImpl")) && WidgetGenBaseClass->HasMetaData(TEXT("WidgetGenImpl")))
		{
			const FString WidgetGenImplModuleName = WidgetGenImplClass->GetOutermost()->GetName().RightChop(FString(TEXT("/Script/")).Len());
			const FString WidgetGenBaseModuleName = WidgetGenBaseClass->GetOutermost()->GetName().RightChop(FString(TEXT("/Script/")).Len());

			auto WidgetGenBaseModule = FindModule(WidgetGenBaseModuleName);
			auto WidgetGenImplModule = FindModule(WidgetGenImplModuleName);
			if (!WidgetGenImplModule.IsSet() || !WidgetGenBaseModule.IsSet())
			{
				return false;
			}

			OutBaseClassInfo.ClassName = WidgetGenBaseClass->GetName();
			OutImplmentClassInfo.ClassName = WidgetGenImplClass->GetName();
			OutBaseClassInfo.ClassModule = *WidgetGenBaseModule;
			OutImplmentClassInfo.ClassModule = *WidgetGenImplModule;
			OutBaseClassInfo.ClassPath = WidgetGenBaseModule->ModuleSourcePath;
			OutImplmentClassInfo.ClassPath = WidgetGenImplModule->ModuleSourcePath;

			return true;
		}
		return false;
	}

	//　class BP_Menu : public UUserWidget
	// 生成されたクラスでは無い場合にはクラスを指定する必要があるレイアウト
	const auto GetModule = []() -> TOptional<FModuleContextInfo>
		{
			TArray<FModuleContextInfo> GameModules = GameProjectUtils::GetCurrentProjectModules();
			if (GameModules.Num() > 0)
			{
				return GameModules[0];
			}
			GameModules = GameProjectUtils::GetCurrentProjectPluginModules();
			if (GameModules.Num() == 0)
			{
				return GameModules[0];
			}
			return NullOpt;
		};

	auto SelectModuleResult = GetModule();
	if (!SelectModuleResult.IsSet())
	{
		return false;
	}

	const FModuleContextInfo SelectModule = SelectModuleResult.GetValue();
	OutBaseClassInfo.ClassName = FString::Printf(TEXT("WidgetGenBase%s"), *BlueprintClassName);
	OutImplmentClassInfo.ClassName = FString::Printf(TEXT("WidgetGenImpl%s"), *BlueprintClassName);
	OutBaseClassInfo.ClassModule = SelectModule;
	OutImplmentClassInfo.ClassModule = SelectModule;
	OutBaseClassInfo.ClassPath = SelectModule.ModuleSourcePath;
	OutImplmentClassInfo.ClassPath = SelectModule.ModuleSourcePath;

	return true;
}


void WidgetGenCodeProjectUtils::GetPropertyInfos(UWidgetBlueprint* InBlueprint, TArray<FObjectProperty*>& OutPropertys, TArray<UClass*>& OutPropertyClasses, TArray<FString>& OutPropertyHeaderFiles)
{
	UClass* Class = InBlueprint->GeneratedClass;

	auto AddProperty = [&](FObjectProperty* InProperty)
		{
			OutPropertys.Add(InProperty);
			OutPropertyClasses.AddUnique(InProperty->PropertyClass);

			FString HeaderPath;
			if (GetClassIncludePath(InProperty->PropertyClass, HeaderPath))
			{
				OutPropertyHeaderFiles.AddUnique(HeaderPath);
			}
		};

	for (FObjectProperty* Property : TFieldRange<FObjectProperty>(Class, EFieldIterationFlags::None))
	{
		const FName PropertyName = Property->GetFName();

		if (InBlueprint->WidgetTree->FindWidget(PropertyName) != nullptr)
		{
			AddProperty(Property);
		}
		for (auto Animation : InBlueprint->Animations)
		{
			if (Animation->GetFName() == PropertyName)
			{
				AddProperty(Property);
			}
		}
	}
}


bool WidgetGenCodeProjectUtils::GenerateClassHeaderFile(
	const FWidgetGenClassInfomation& ClassInfo,
	const FNewClassInfo ParentClassInfo,
	const FString& HeaderTemplateFile,
	const FString& OriginalAssetPath,
	const FString& ClassProperties,
	const FString& ClassForwardDeclaration,
	FString& OutSyncLocation,
	FText& OutFailReason)
{
	const FString& NewHeaderFileName = ClassInfo.ClassHeaderPath;
	const FString& UnPrefixedClassName = ClassInfo.ClassName;
	const FModuleContextInfo& ModuleInfo = ClassInfo.ClassModule;

	FString Template;

	if (!ReadWidgetGenTemplateFile(HeaderTemplateFile, Template, OutFailReason))
	{
		return false;
	}

	const FString ClassPrefix = TEXT("U");
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;
	const FString BaseClassName = ParentClassInfo.BaseClass ? ParentClassInfo.BaseClass->GetName() : TEXT("");
	const FString PrefixedBaseClassName = ClassPrefix + BaseClassName;

	FString BaseClassIncludeDirective;
	FString BaseClassIncludePath;
	if (GetClassIncludePath(ParentClassInfo.BaseClass, BaseClassIncludePath))
	{
		BaseClassIncludeDirective = FString::Printf(IncludePathFormatString, *BaseClassIncludePath);
	}

	FString ModuleApiMacro;
	{
		GameProjectUtils::EClassLocation ClassPathLocation = GameProjectUtils::EClassLocation::UserDefined;
		if (GameProjectUtils::GetClassLocation(NewHeaderFileName, ModuleInfo, ClassPathLocation))
		{
			// If this class isn't Private, make sure and include the API macro so it can be linked within other modules
			if (ClassPathLocation != GameProjectUtils::EClassLocation::Private)
			{
				ModuleApiMacro = ModuleInfo.ModuleName.ToUpper() + "_API "; // include a trailing space for the template formatting
			}
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_MODULE_API_MACRO%"), *ModuleApiMacro, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ORIGINAL_ASSET_PATH%"), *OriginalAssetPath, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_BASE_CLASS_NAME%"), *PrefixedBaseClassName, ESearchCase::CaseSensitive);

	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_FORWARD_DECLARATION%"), *ClassForwardDeclaration, ESearchCase::CaseSensitive);

	// Special case where where the wildcard starts with a tab and ends with a new line
	constexpr bool bLeadingTab = true;
	constexpr bool bTrailingNewLine = true;
	FinalOutput = ReplaceWildcard(FinalOutput, TEXT("%CLASS_PROPERTIES%"), *ClassProperties, bLeadingTab, bTrailingNewLine);
	if (BaseClassIncludeDirective.Len() == 0)
	{
		FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%") LINE_TERMINATOR, TEXT(""), ESearchCase::CaseSensitive);
	}
	FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%"), *BaseClassIncludeDirective, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation(FinalOutput, OutSyncLocation);

	return GameProjectUtils::WriteOutputFile(NewHeaderFileName, FinalOutput, OutFailReason);
}

bool WidgetGenCodeProjectUtils::GenerateClassSourceFile(
	const FWidgetGenClassInfomation& ClassInfo,
	const FNewClassInfo ParentClassInfo,
	const FString& SourceTemplateFile,
	const FString& AdditionalIncludeDirectives,
	const FString& ClassMemberInitialized,
	FString& OutSyncLocation,
	FText& OutFailReason)
{
	const FString& NewSourceFileName = ClassInfo.ClassSourcePath;
	const FString& UnPrefixedClassName = ClassInfo.ClassName;
	const FModuleContextInfo& ModuleInfo = ClassInfo.ClassModule;

	FString Template;

	if (!ReadWidgetGenTemplateFile(SourceTemplateFile, Template, OutFailReason))
	{
		return false;
	}

	const FString ClassPrefix = TEXT("U");
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;

	FString BaseClassIncludeDirective;
	FString BaseClassIncludePath;
	if (GetClassIncludePath(ParentClassInfo.BaseClass, BaseClassIncludePath))
	{
		BaseClassIncludeDirective = FString::Printf(IncludePathFormatString, *BaseClassIncludePath);
	}

	// Calculate the correct include path for the module header
	FString PchIncludeDirective;
	if (IsUsingOldStylePch(ModuleInfo.ModuleSourcePath))
	{
		const FString ModuleIncludePath = GameProjectUtils::DetermineModuleIncludePath(ModuleInfo, NewSourceFileName);
		if (ModuleIncludePath.Len() > 0)
		{
			PchIncludeDirective = FString::Printf(IncludePathFormatString, *ModuleIncludePath);
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ADDITIONAL_INCLUDE_DIRECTIVES%"), *AdditionalIncludeDirectives, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PCH_INCLUDE_DIRECTIVE%"), *PchIncludeDirective, ESearchCase::CaseSensitive);

	// Fixup the header file include for this cpp file
	{
		const FString RelativeHeaderIncludePath = GetIncludePathForFile(NewSourceFileName, ModuleInfo.ModuleSourcePath);

		// First make sure we remove any potentially incorrect, legacy paths generated from some version of #include "%UNPREFIXED_CLASS_NAME%.h"
		// This didn't take into account potential subfolders for the created class
		const FString LegacyHeaderInclude = FString::Printf(IncludePathFormatString, *FPaths::GetCleanFilename(RelativeHeaderIncludePath));
		FinalOutput = FinalOutput.Replace(*LegacyHeaderInclude, TEXT(""), ESearchCase::CaseSensitive);

		// Now add the correct include directive which may include a subfolder.
		const FString HeaderIncludeDirective = FString::Printf(IncludePathFormatString, *RelativeHeaderIncludePath);
		FinalOutput = FinalOutput.Replace(TEXT("%MY_HEADER_INCLUDE_DIRECTIVE%"), *HeaderIncludeDirective, ESearchCase::CaseSensitive);
	}

	// Special case where where the wildcard starts with a tab and ends with a new line
	constexpr bool bLeadingTab = true;
	constexpr bool bTrailingNewLine = true;
	FinalOutput = ReplaceWildcard(FinalOutput, TEXT("%CLASS_MEMBER_INITIALIZED%"), *ClassMemberInitialized, bLeadingTab, bTrailingNewLine);
	if (BaseClassIncludeDirective.Len() == 0)
	{
		FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%") LINE_TERMINATOR, TEXT(""), ESearchCase::CaseSensitive);
	}
	FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%"), *BaseClassIncludeDirective, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation(FinalOutput, OutSyncLocation);

	return GameProjectUtils::WriteOutputFile(NewSourceFileName, FinalOutput, OutFailReason);
}

#if 0

GameProjectUtils::EAddCodeToProjectResult AddCodeToProject_Internal(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason, GameProjectUtils::EReloadStatus& OutReloadStatus)
{
	if (!ParentClassInfo.IsSet())
	{
		OutFailReason = LOCTEXT("MissingParentClass", "You must specify a parent class");
		return GameProjectUtils::EAddCodeToProjectResult::InvalidInput;
	}

	const FString CleanClassName = ParentClassInfo.GetCleanClassName(NewClassName);
	const FString FinalClassName = ParentClassInfo.GetFinalClassName(NewClassName);

	if (!GameProjectUtils::IsValidClassNameForCreation(FinalClassName, ModuleInfo, DisallowedHeaderNames, OutFailReason))
	{
		return GameProjectUtils::EAddCodeToProjectResult::InvalidInput;
	}

	if (!FApp::HasProjectName())
	{
		OutFailReason = LOCTEXT("AddCodeToProject_NoGameName", "You can not add code because you have not loaded a project.");
		return GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode;
	}

	FString NewHeaderPath;
	FString NewCppPath;
	if (!GameProjectUtils::CalculateSourcePaths(NewClassPath, ModuleInfo, NewHeaderPath, NewCppPath, &OutFailReason))
	{
		return GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode;
	}

	FScopedSlowTask SlowTask(7, LOCTEXT("AddingCodeToProject", "Adding code to project..."));
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame();

	auto RequiredDependencies = GameProjectUtils::GetRequiredAdditionalDependencies(ParentClassInfo);
	RequiredDependencies.Remove(ModuleInfo.ModuleName);

	// Update project file if needed.
	auto bUpdateProjectModules = false;

	// If the project does not already contain code, add the primary game module
	TArray<FString> CreatedFiles;
	TArray<FString> StartupModuleNames;

	const bool bProjectHadCodeFiles = GameProjectUtils::ProjectHasCodeFiles();
	if (!bProjectHadCodeFiles)
	{
		// Delete any generated intermediate code files. This ensures that blueprint projects with custom build settings can be converted to code projects without causing errors.
		IFileManager::Get().DeleteDirectory(*(FPaths::ProjectIntermediateDir() / TEXT("Source")), false, true);

		// We always add the basic source code to the root directory, not the potential sub-directory provided by NewClassPath
		const FString SourceDir = FPaths::GameSourceDir().LeftChop(1); // Trim the trailing /

		// Assuming the game name is the same as the primary game module name
		const FString GameModuleName = FApp::GetProjectName();

		if (GameProjectUtils::GenerateBasicSourceCode(SourceDir, GameModuleName, FPaths::ProjectDir(), StartupModuleNames, CreatedFiles, OutFailReason))
		{
			bUpdateProjectModules = true;
		}
		else
		{
			GameProjectUtils::DeleteCreatedFiles(SourceDir, CreatedFiles);
			return GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode;
		}
	}

	if (RequiredDependencies.Num() > 0 || bUpdateProjectModules)
	{
		GameProjectUtils::UpdateProject(
			FProjectDescriptorModifier::CreateLambda(
				[&StartupModuleNames, &RequiredDependencies, &ModuleInfo, bUpdateProjectModules](FProjectDescriptor& Descriptor)
				{
					bool bNeedsUpdate = false;

					bNeedsUpdate |= GameProjectUtils::UpdateStartupModuleNames(Descriptor, bUpdateProjectModules ? &StartupModuleNames : nullptr);
					bNeedsUpdate |= GameProjectUtils::UpdateRequiredAdditionalDependencies(Descriptor, RequiredDependencies, ModuleInfo.ModuleName);

					return bNeedsUpdate;
				}));
	}

	SlowTask.EnterProgressFrame();

	// Class Header File
	const FString NewHeaderFilename = NewHeaderPath / ParentClassInfo.GetHeaderFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		TArray<FString> ClassSpecifiers;

		// Set UCLASS() specifiers based on parent class type. Currently, only UInterface uses this.
		if (ParentClassInfo.ClassType == FNewClassInfo::EClassType::UInterface)
		{
			ClassSpecifiers.Add(TEXT("MinimalAPI"));
		}

#if 0
		if (GameProjectUtils::GenerateClassHeaderFile(NewHeaderFilename, CleanClassName, ParentClassInfo, ClassSpecifiers, TEXT(""), TEXT(""), UnusedSyncLocation, ModuleInfo, false, OutFailReason))
		{
			CreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			GameProjectUtils::DeleteCreatedFiles(NewHeaderPath, CreatedFiles);
			return GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode;
		}
#endif
	}

	SlowTask.EnterProgressFrame();

#if 0
	// Class CPP file
	const FString NewCppFilename = NewCppPath / ParentClassInfo.GetSourceFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		if (GameProjectUtils::GenerateClassCPPFile(NewCppFilename, CleanClassName, ParentClassInfo, TArray<FString>(), TArray<FString>(), TEXT(""), UnusedSyncLocation, ModuleInfo, OutFailReason))
		{
			CreatedFiles.Add(NewCppFilename);
		}
		else
		{
			GameProjectUtils::DeleteCreatedFiles(NewCppPath, CreatedFiles);
			return GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode;
		}
	}
#endif

	SlowTask.EnterProgressFrame();

	TArray<FString> CreatedFilesForExternalAppRead;
	CreatedFilesForExternalAppRead.Reserve(CreatedFiles.Num());
	for (const FString& CreatedFile : CreatedFiles)
	{
		CreatedFilesForExternalAppRead.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CreatedFile));
	}

	bool bGenerateProjectFiles = true;

	// First see if we can avoid a full generation by adding the new files to an already open project
	if (bProjectHadCodeFiles && FSourceCodeNavigation::AddSourceFiles(CreatedFilesForExternalAppRead))
	{
		// We managed the gather, so we can skip running the full generate
		bGenerateProjectFiles = false;
	}

	if (bGenerateProjectFiles)
	{
		// Generate project files if we happen to be using a project file.
		if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
		{
			OutFailReason = LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files.");
			return GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload;
		}
	}

	SlowTask.EnterProgressFrame();

	// Mark the files for add in SCC
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable())
	{
		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), CreatedFilesForExternalAppRead);
	}

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("CompilingCPlusPlusCode", "Compiling new C++ code.  Please wait..."));

	OutHeaderFilePath = NewHeaderFilename;
	OutCppFilePath = NewCppFilename;
	OutReloadStatus = GameProjectUtils::EReloadStatus::NotReloaded;

	return WidgetGenCodeProjectUtils::ProjectRecompileModule(ModuleInfo, bProjectHadCodeFiles, OutReloadStatus, OutFailReason);
}


#endif

GameProjectUtils::EAddCodeToProjectResult WidgetGenCodeProjectUtils::AddProjectFiles(
	const TArray<FString>& CreatedFiles,
	bool bProjectHadCodeFiles,
	FText& OutFailReason,
	FSlowTask* SlowTask)
{
	TArray<FString> CreatedFilesForExternalAppRead;
	CreatedFilesForExternalAppRead.Reserve(CreatedFiles.Num());
	for (const FString& CreatedFile : CreatedFiles)
	{
		CreatedFilesForExternalAppRead.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CreatedFile));
	}

	bool bGenerateProjectFiles = true;

	// First see if we can avoid a full generation by adding the new files to an already open project
	if (bProjectHadCodeFiles && FSourceCodeNavigation::AddSourceFiles(CreatedFilesForExternalAppRead))
	{
		// We managed the gather, so we can skip running the full generate
		bGenerateProjectFiles = false;
	}

	if (bGenerateProjectFiles)
	{
		// Generate project files if we happen to be using a project file.
		if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
		{
			OutFailReason = LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files.");
			return GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload;
		}
	}

	if (SlowTask)
	{
		SlowTask->EnterProgressFrame();
	}

	// Mark the files for add in SCC
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable())
	{
		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), CreatedFilesForExternalAppRead);
	}

	return GameProjectUtils::EAddCodeToProjectResult::Succeeded;
}

GameProjectUtils::EAddCodeToProjectResult WidgetGenCodeProjectUtils::ProjectRecompileModule(
	const FModuleContextInfo& ModuleInfo,
	bool bProjectHadCodeFiles,
	GameProjectUtils::EReloadStatus& OutReloadStatus,
	FText& OutFailReason) 
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (LiveCoding != nullptr && LiveCoding->IsEnabledForSession())
	{
		if (!bProjectHadCodeFiles)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LiveCodingNoSources", "Project now includes sources, please close the editor and build from your IDE."));
			return GameProjectUtils::EAddCodeToProjectResult::Succeeded;
		}

		if (LiveCoding->AutomaticallyCompileNewClasses())
		{
			LiveCoding->Compile(ELiveCodingCompileFlags::WaitForCompletion, nullptr);
			OutReloadStatus = GameProjectUtils::EReloadStatus::Reloaded;
		}
		return GameProjectUtils::EAddCodeToProjectResult::Succeeded;
	}
#endif

	if (!bProjectHadCodeFiles)
	{
		// This is the first time we add code to this project so compile its game DLL
		const FString GameModuleName = FApp::GetProjectName();
		check(ModuleInfo.ModuleName == GameModuleName);

		// Because this project previously didn't have any code, the UBT target name will just be UnrealEditor. Now that we've
		// added some code, the target name will be changed to match the editor target for the new source. 
		FString NewUBTTargetName = GameModuleName + TEXT("Editor");
		FPlatformMisc::SetUBTTargetName(*NewUBTTargetName);

		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
		if (!HotReloadSupport.RecompileModule(*GameModuleName, *GWarn, ERecompileModuleFlags::ReloadAfterRecompile | ERecompileModuleFlags::ForceCodeProject))
		{
			OutFailReason = LOCTEXT("FailedToCompileNewGameModule", "Failed to compile newly created game module.");
			return GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload;
		}

		// Notify that we've created a brand new module
		FSourceCodeNavigation::AccessOnNewModuleAdded().Broadcast(*GameModuleName);
		OutReloadStatus = GameProjectUtils::EReloadStatus::Reloaded;
	}
	else if (GetDefault<UEditorPerProjectUserSettings>()->bAutomaticallyHotReloadNewClasses)
	{
		FModuleStatus ModuleStatus;
		const FName ModuleFName = *ModuleInfo.ModuleName;
		if (ensure(FModuleManager::Get().QueryModule(ModuleFName, ModuleStatus)))
		{
			// Compile the module that the class was added to so that the newly added class with appear in the Content Browser
			TArray<UPackage*> PackagesToRebind;
			if (ModuleStatus.bIsLoaded)
			{
				const bool bIsHotReloadable = FModuleManager::Get().DoesLoadedModuleHaveUObjects(ModuleFName);
				if (bIsHotReloadable)
				{
					// Is there a UPackage with the same name as this module?
					const FString PotentialPackageName = FString(TEXT("/Script/")) + ModuleInfo.ModuleName;
					UPackage* Package = FindPackage(nullptr, *PotentialPackageName);
					if (Package)
					{
						PackagesToRebind.Add(Package);
					}
				}
			}

			IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
			if (PackagesToRebind.Num() > 0)
			{
				// Perform a hot reload
				ECompilationResult::Type CompilationResult = HotReloadSupport.RebindPackages(PackagesToRebind, EHotReloadFlags::WaitForCompletion, *GWarn);
				if (CompilationResult != ECompilationResult::Succeeded && CompilationResult != ECompilationResult::UpToDate)
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToHotReloadModuleFmt", "Failed to automatically hot reload the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload;
				}
			}
			else
			{
				// Perform a regular unload, then reload
				if (!HotReloadSupport.RecompileModule(ModuleFName, *GWarn, ERecompileModuleFlags::ReloadAfterRecompile))
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToCompileModuleFmt", "Failed to automatically compile the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload;
				}
			}
		}
		OutReloadStatus = GameProjectUtils::EReloadStatus::Reloaded;
	}

	return GameProjectUtils::EAddCodeToProjectResult::Succeeded;
}


TSharedPtr<FBlueprintEditor> WidgetGenCodeProjectUtils::GetBlueprintEditor(UWidgetBlueprint* WidgetBlueprint, bool bOpenEditor)
{
	check(WidgetBlueprint);

	TSharedPtr<FBlueprintEditor> ResultEditor;
	if (WidgetBlueprint != nullptr)
	{
		if (bOpenEditor)
		{
			// @todo toolkit major: Needs world-centric support
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(WidgetBlueprint);
		}

		TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(WidgetBlueprint);
		// If we found a BlueprintEditor
		if (FoundAssetEditor.IsValid())
		{
			ResultEditor = StaticCastSharedPtr<FBlueprintEditor>(FoundAssetEditor);
		}
	}
	return ResultEditor;
}



#undef LOCTEXT_NAMESPACE
