// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetGenCodeToolCommands.h"

#define LOCTEXT_NAMESPACE "WidgetGenCodeTool"

void FWidgetGenCodeToolCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "WidgetGenCodeTool", "Execute WidgetGenCodeTool action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
