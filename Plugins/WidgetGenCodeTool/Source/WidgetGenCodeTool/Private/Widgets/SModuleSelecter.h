// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

struct FModuleContextInfo;

class SModuleSelecter : public SCompoundWidget
{
	using FOnSelectionChanged = typename TSlateDelegates< TSharedPtr<FModuleContextInfo> >::FOnSelectionChanged;

public:
	SLATE_BEGIN_ARGS(SModuleSelecter)
		: _OnSelectionChanged()
		{}

		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

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

	FOnSelectionChanged OnSelectionChanged;

};
