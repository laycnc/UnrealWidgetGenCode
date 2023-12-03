// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

struct FModuleContextInfo;

class SWidgetGenCodeToolDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetGenCodeToolDialog)
		{}

	SLATE_END_ARGS()


public:

	void Construct(const FArguments& InArgs);

private:

	FText GetSelectedModuleComboText() const;
	void SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value);


private:
	TSharedPtr<SComboBox<TSharedPtr<FModuleContextInfo>>> AvailableModulesCombo;
	TArray<TSharedPtr<FModuleContextInfo>> AvailableModules;
	TSharedPtr<FModuleContextInfo> SelectedModuleInfo;


};
