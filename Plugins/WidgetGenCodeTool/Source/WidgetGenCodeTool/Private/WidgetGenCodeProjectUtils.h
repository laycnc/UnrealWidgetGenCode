// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AddToProjectConfig.h"
#include "ModuleDescriptor.h"
#include "GameProjectUtils.h"

class UWidgetBlueprint;
struct FModuleContextInfo;
struct FSlowTask;

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

	static bool GenWidgetWidgetInfo(UWidgetBlueprint* InBlueprint, FWidgetGenClassInfomation& OutBaseClassInfo, FWidgetGenClassInfomation& OutImplmentClassInfo);

	static void GetPropertyInfos(UWidgetBlueprint* InBlueprint, TArray<FObjectProperty*>& OutPropertys, TArray<UClass*>& OutPropertyClasses, TArray<FString>& OutPropertyHeaderFiles);

	static bool GenerateClassHeaderFile(
		const FWidgetGenClassInfomation& ClassInfo,
		const FNewClassInfo ParentClassInfo,
		const TArray<FString>& ClassSpecifierList,
		const FString& ClassProperties,
		const FString& ClassForwardDeclaration,
		FString& OutSyncLocation,
		FText& OutFailReason);


	static bool GenerateClassSourceFile(
		const FWidgetGenClassInfomation& ClassInfo,
		const FNewClassInfo ParentClassInfo,
		const FString& AdditionalIncludeDirectives,
		const FString& ClassMemberInitialized,
		FString& OutSyncLocation,
		FText& OutFailReason);


#if 0

	static GameProjectUtils::EAddCodeToProjectResult AddCodeToProject_Internal(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason, GameProjectUtils::EReloadStatus& OutReloadStatus);

#endif

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

};
