#include "WidgetGenCodeProjectUtils.h"
#include "GameProjectUtils.h"
#include "ClassTemplateEditorSubsystem.h"
#include "Editor.h"
#include "GeneralProjectSettings.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "SourceCodeNavigation.h"
#include "PluginBlueprintLibrary.h"

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

bool WidgetGenCodeProjectUtils::GenerateClassHeaderFile(
	const FWidgetGenClassInfomation& ClassInfo,
	const FNewClassInfo ParentClassInfo,
	const TArray<FString>& ClassSpecifierList,
	const FString& ClassProperties,
	const FString& ClassForwardDeclaration,
	FString& OutSyncLocation,
	FText& OutFailReason)
{
	const FString& NewHeaderFileName = ClassInfo.ClassHeaderPath;
	const FString& UnPrefixedClassName = ClassInfo.ClassName;
	const FModuleContextInfo& ModuleInfo = ClassInfo.ClassModule;

	FString Template;

	if (!ReadWidgetGenTemplateFile(TEXT("WidgetGenClass.h.template"), Template, OutFailReason))
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

	FString EventualConstructorDeclaration;

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_MODULE_API_MACRO%"), *ModuleApiMacro, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UCLASS_SPECIFIER_LIST%"), *GameProjectUtils::MakeCommaDelimitedList(ClassSpecifierList, false), ESearchCase::CaseSensitive);
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

#undef LOCTEXT_NAMESPACE
