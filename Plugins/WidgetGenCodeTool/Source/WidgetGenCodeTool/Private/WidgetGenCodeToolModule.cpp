// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetGenCodeToolModule.h"
#include "Modules/ModuleManager.h"
#include "BlueprintEditorModule.h"
#include "WidgetBlueprint.h"
#include "WidgetGenCodeToolCommands.h"
#include "WidgetGenCodeToolStyle.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/SWidgetGenCodeToolDialog.h"

#include "Widgets/Input/SCheckBox.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"

#define LOCTEXT_NAMESPACE "FWidgetGenCodeToolModule"

void FWidgetGenCodeToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FWidgetGenCodeToolStyle::Initialize();
	FWidgetGenCodeToolStyle::ReloadTextures();

	FWidgetGenCodeToolCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	//PluginCommands->MapAction(FWidgetGenCodeToolCommands::Get().PluginAction, FExecuteAction::CreateRaw(this, &FWidgetGenCodeToolModule::OnPluginAction), FCanExecuteAction());


	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::Get().GetModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));

	GatherBlueprintMenuExtensionsHandle = BlueprintEditorModule.OnGatherBlueprintMenuExtensions().AddRaw(this, &FWidgetGenCodeToolModule::OnGatherBlueprintMenuExtensions);

}

void FWidgetGenCodeToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::Get().GetModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
	BlueprintEditorModule.OnGatherBlueprintMenuExtensions().Remove(GatherBlueprintMenuExtensionsHandle);


	FWidgetGenCodeToolCommands::Unregister();
	FWidgetGenCodeToolStyle::Shutdown();
}

void FWidgetGenCodeToolModule::OnGatherBlueprintMenuExtensions(TSharedPtr<FExtender> InExtender, UBlueprint* InBlueprint)
{
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(InBlueprint);
	if (!IsValid(WidgetBlueprint))
	{
		// UMG以外のBlueprintは無視する
		return;
	}

	InExtender->AddToolBarExtension(
		TEXT("Asset"),
		EExtensionHook::After,
		PluginCommands,
		FToolBarExtensionDelegate::CreateRaw(this, &FWidgetGenCodeToolModule::OnToolBarExtension, WidgetBlueprint));
}

void FWidgetGenCodeToolModule::OnToolBarExtension(FToolBarBuilder& InBuilder, UWidgetBlueprint* InBlueprint)
{
	InBuilder.BeginSection("Command");
	{
		//InBuilder.AddToolBarButton(FWidgetGenCodeToolCommands::Get().PluginAction);
		InBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateRaw(this, &FWidgetGenCodeToolModule::OnPluginAction, InBlueprint)), 
			NAME_None, 
			LOCTEXT("OpenDialog", "Open Dialog"));
	}
	InBuilder.EndSection();
}

void FWidgetGenCodeToolModule::OnPluginAction(UWidgetBlueprint* InBlueprint)
{
	UClass* Class = InBlueprint->GeneratedClass;

	TArray<FObjectProperty*> Propertys;
	for (FObjectProperty* Property : TFieldRange<FObjectProperty>(Class, EFieldIterationFlags::None))
	{
		const FName PropertyName = Property->GetFName();

		if (UWidget* Widget = InBlueprint->WidgetTree->FindWidget(PropertyName))
		{
			Propertys.Add(Property);
		}

		for (auto Animation : InBlueprint->Animations)
		{
			if (Animation->GetFName() == PropertyName)
			{
				Propertys.Add(Property);
			}
		}
	}

	// If we've been given a class then we only show the second page of the dialog, so we can make the window smaller as that page doesn't have as much content
	const FVector2D WindowSize = FVector2D(940, 480);

	FText WindowTitle = LOCTEXT("AddCodeWindowHeader_Blueprint", "Add Blueprint Class");

	TSharedRef<SWindow> AddCodeWindow =
		SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize)
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMinimize(false).SupportsMaximize(false);

	TSharedRef<SWidgetGenCodeToolDialog> NewClassDialog =
		SNew(SWidgetGenCodeToolDialog);

	AddCodeWindow->SetContent(NewClassDialog);


	static const FName MainFrameModuleName = "MainFrame";
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrameModuleName);
	TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();

	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(AddCodeWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AddCodeWindow);
	}

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWidgetGenCodeToolModule, WidgetGenCodeTool);
