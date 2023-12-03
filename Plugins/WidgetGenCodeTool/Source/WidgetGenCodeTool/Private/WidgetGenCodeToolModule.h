// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUICommandList;
class FToolBarBuilder;
class FMenuBuilder;
class UWidgetBlueprint;

class FWidgetGenCodeToolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnGatherBlueprintMenuExtensions(TSharedPtr<FExtender> InEctender, UBlueprint* InBlueprint);
	void OnToolBarExtension(FToolBarBuilder& InBuilder, UWidgetBlueprint* InBlueprint);
	void OnPluginAction(UWidgetBlueprint* InBlueprint);
private:
	FDelegateHandle GatherBlueprintMenuExtensionsHandle;
	TSharedPtr<FUICommandList> PluginCommands;

};
