// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetGenCodeToolStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FWidgetGenCodeToolStyle::StyleInstance = nullptr;

void FWidgetGenCodeToolStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FWidgetGenCodeToolStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FWidgetGenCodeToolStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WidgetGenCodeToolStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FWidgetGenCodeToolStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("WidgetGenCodeToolStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("WidgetGenCodeTool")->GetBaseDir() / TEXT("Resources"));

	Style->Set("WidgetGenCodeTool.PluginAction", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));
	return Style;
}

void FWidgetGenCodeToolStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FWidgetGenCodeToolStyle::Get()
{
	return *StyleInstance;
}
