// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AddToProjectConfig.h"
#include "ModuleDescriptor.h"
#include "GameProjectUtils.h"

class UWidgetBlueprint;
struct FModuleContextInfo;
struct FSlowTask;
class FBlueprintEditor;

struct FWidgetGenClassInfomation
{
	FString ClassName;
	FString ClassPath;
	FString ClassHeaderPath;
	FString ClassSourcePath;
	FModuleContextInfo ClassModule;
};

struct WidgetGenCodeProjectUtils
{
public:

	static bool GenWidgetWidgetInfo(
		UWidgetBlueprint* InBlueprint,
		FWidgetGenClassInfomation& OutBaseClassInfo, 
		FWidgetGenClassInfomation& OutImplmentClassInfo,
		UClass*& OutOriginalBaseClass,
		bool& OutGenCodeFileExists);

	static void GetPropertyInfos(UWidgetBlueprint* InBlueprint, TArray<FObjectProperty*>& OutPropertys, TArray<UClass*>& OutPropertyClasses, TArray<FString>& OutPropertyHeaderFiles);


	static void CreateBaseClassParam(
		UWidgetBlueprint* InBlueprint,
		FString& ClassProperties,
		FString& ClassForwardDeclaration,
		FString& ClassMemberInitialized,
		FString& AdditionalIncludeDirectives
	);

	static bool GenerateClass(
		const FWidgetGenClassInfomation& InClassInfo,
		UClass* ParentClass,
		const FString& OriginalAssetPath,
		const FString& ClassProperties,
		const FString& ClassForwardDeclaration,
		const FString& AdditionalIncludeDirectives,
		const FString& ClassMemberInitialized,
		const FString& InHeaderTemplate, 
		const FString& InSourceTemplate,
		FString& OutSyncLocation,
		FText& OutFailReason,
		TArray<FString>& OutCreatedFiles,
		FSlowTask* SlowTask = nullptr
		);

	static bool GenerateClassHeaderFile(
		const FWidgetGenClassInfomation& ClassInfo,
		const FNewClassInfo ParentClassInfo,
		const FString& HeaderTemplateFile,
		const FString& OOriginalAssetPath,
		const FString& ClassProperties,
		const FString& ClassForwardDeclaration,
		FString& OutSyncLocation,
		FText& OutFailReason);


	static bool GenerateClassSourceFile(
		const FWidgetGenClassInfomation& ClassInfo,
		const FNewClassInfo ParentClassInfo,
		const FString& SourceTemplateFile,
		const FString& AdditionalIncludeDirectives,
		const FString& ClassMemberInitialized,
		FString& OutSyncLocation,
		FText& OutFailReason);

	static GameProjectUtils::EAddCodeToProjectResult AddProjectFiles(
		const TArray<FString>& CreatedFiles,
		bool bProjectHadCodeFiles,
		FText& OutFailReason,
		FSlowTask* SlowTask = nullptr);

	static GameProjectUtils::EAddCodeToProjectResult ProjectRecompileModule(
		const FModuleContextInfo& ModuleInfo,
		bool bProjectHadCodeFiles,
		GameProjectUtils::EReloadStatus& OutReloadStatus,
		FText& OutFailReason);


	static TSharedPtr<FBlueprintEditor> GetBlueprintEditor(UWidgetBlueprint* WidgetBlueprint, bool bOpenEditor = true);
};
