// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SModuleSelecter.h"
#include "GameProjectUtils.h"

#define LOCTEXT_NAMESPACE "SModuleSelecter"

void SModuleSelecter::Construct(const FArguments& InArgs)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;

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

		OnSelectionChanged.ExecuteIfBound(SelectedModuleInfo, ESelectInfo::Direct);
	}

	ChildSlot
		[
			SAssignNew(AvailableModulesCombo, SComboBox<TSharedPtr<FModuleContextInfo>>)
				.Visibility(EVisibility::Visible)
				.ToolTipText(LOCTEXT("ModuleComboToolTip", "Choose the target module for your new class"))
				.OptionsSource(&AvailableModules)
				.InitiallySelectedItem(SelectedModuleInfo)
				.OnSelectionChanged(this, &SModuleSelecter::SelectedModuleComboBoxSelectionChanged)
				.OnGenerateWidget(this, &SModuleSelecter::MakeWidgetForSelectedModuleCombo)
				[
					SNew(STextBlock)
						.Text(this, &SModuleSelecter::GetSelectedModuleComboText)
				]
		];
}

FText SModuleSelecter::GetSelectedModuleComboText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(SelectedModuleInfo->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(SelectedModuleInfo->ModuleType)));
	return FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args);
}

void SModuleSelecter::SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo)
{
	const FString& OldModulePath = SelectedModuleInfo->ModuleSourcePath;
	const FString& NewModulePath = Value->ModuleSourcePath;

	SelectedModuleInfo = Value;

	// 変更を通知する
	OnSelectionChanged.ExecuteIfBound(Value, ESelectInfo::Direct);
}

TSharedRef<SWidget> SModuleSelecter::MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(Value->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(Value->ModuleType)));
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args));
}

#undef LOCTEXT_NAMESPACE
