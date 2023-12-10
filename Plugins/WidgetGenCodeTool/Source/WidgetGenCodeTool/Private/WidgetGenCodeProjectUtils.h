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
	FModuleContextInfo ClassModule;
};

struct WidgetGenCodeProjectUtils
{
public:

	static bool GenWidgetWidgetInfo(UWidgetBlueprint* InBlueprint, FWidgetGenClassInfomation& OutBaseClassInfo, FWidgetGenClassInfomation& OutImplmentClassInfo);

#if 0
	bool GenerateClassHeaderFile(const FString& NewHeaderFileName, const FString UnPrefixedClassName, const FNewClassInfo ParentClassInfo, const TArray<FString>& ClassSpecifierList, const FString& ClassProperties, const FString& ClassFunctionDeclarations, FString& OutSyncLocation, const FModuleContextInfo& ModuleInfo, bool bDeclareConstructor, FText& OutFailReason);
#endif

	static void Test(UWidgetBlueprint* InBlueprint);

};
