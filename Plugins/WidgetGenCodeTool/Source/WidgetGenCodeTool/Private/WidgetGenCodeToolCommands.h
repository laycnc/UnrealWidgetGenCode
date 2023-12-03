// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "WidgetGenCodeToolStyle.h"

class FWidgetGenCodeToolCommands : public TCommands<FWidgetGenCodeToolCommands>
{
public:

	FWidgetGenCodeToolCommands()
		: TCommands<FWidgetGenCodeToolCommands>(TEXT("WidgetGenCodeTool"), NSLOCTEXT("Contexts", "WidgetGenCodeTool", "WidgetGenCodeTool Plugin"), NAME_None, FWidgetGenCodeToolStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
