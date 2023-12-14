// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AddToProjectConfig.h"
#include "ModuleDescriptor.h"

class UWidgetBlueprint;
struct FModuleContextInfo;

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


};
