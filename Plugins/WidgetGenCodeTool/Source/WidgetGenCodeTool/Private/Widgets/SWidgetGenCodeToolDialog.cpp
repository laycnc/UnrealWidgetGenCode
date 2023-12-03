// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetGenCodeToolDialog.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "ModuleDescriptor.h"
#include "GameProjectUtils.h"

#define LOCTEXT_NAMESPACE "FWidgetGenCodeToolModule"

void SWidgetGenCodeToolDialog::Construct(const FArguments& InArgs)
{
	// モジュールの一覧を取得する
	{
		TArray<FModuleContextInfo> CurrentModules = GameProjectUtils::GetCurrentProjectModules();
		check(CurrentModules.Num()); // this should never happen since GetCurrentProjectModules is supposed to add a dummy runtime module if the project currently has no modules

		TArray<FModuleContextInfo> CurrentPluginModules = GameProjectUtils::GetCurrentProjectPluginModules();

		CurrentModules.Append(CurrentPluginModules);

		AvailableModules.Reserve(CurrentModules.Num());
		for (const FModuleContextInfo& ModuleInfo : CurrentModules)
		{
			AvailableModules.Emplace(MakeShareable(new FModuleContextInfo(ModuleInfo)));
		}

		Algo::SortBy(AvailableModules, &FModuleContextInfo::ModuleName);

		SelectedModuleInfo = AvailableModules[0];
	}


	ChildSlot
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SAssignNew(AvailableModulesCombo, SComboBox<TSharedPtr<FModuleContextInfo>>)
						.ToolTipText(LOCTEXT("ModuleComboToolTip", "Choose the target module for your new class"))
						.OptionsSource(&AvailableModules)
						.InitiallySelectedItem(SelectedModuleInfo)
						.OnSelectionChanged(this, &SWidgetGenCodeToolDialog::SelectedModuleComboBoxSelectionChanged)
						.OnGenerateWidget(this, &SWidgetGenCodeToolDialog::MakeWidgetForSelectedModuleCombo)
						[
							SNew(STextBlock)
								.Text(this, &SWidgetGenCodeToolDialog::GetSelectedModuleComboText)
						]
				]
		];
}

FText SWidgetGenCodeToolDialog::GetSelectedModuleComboText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(SelectedModuleInfo->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(SelectedModuleInfo->ModuleType)));
	return FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args);
}

void SWidgetGenCodeToolDialog::SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo)
{
	const FString& OldModulePath = SelectedModuleInfo->ModuleSourcePath;
	const FString& NewModulePath = Value->ModuleSourcePath;

	SelectedModuleInfo = Value;
}

TSharedRef<SWidget> SWidgetGenCodeToolDialog::MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(Value->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(Value->ModuleType)));
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args));
}


#undef LOCTEXT_NAMESPACE
