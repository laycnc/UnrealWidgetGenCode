// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "AddToProjectConfig.h"
#include "WidgetGenCodeProjectUtils.h"

class SWizard;
struct FModuleContextInfo;
class UWidgetBlueprint;

class SWidgetGenCodeToolDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetGenCodeToolDialog)
		{}

		/** Event called when code is successfully added to the project */
		SLATE_EVENT(FOnAddedToProject, OnAddedToProject)

		SLATE_ARGUMENT(UWidgetBlueprint*, WidgetBlueprint)
		SLATE_ARGUMENT(FWidgetGenClassInfomation, BaseClassInfo)
		SLATE_ARGUMENT(FWidgetGenClassInfomation, ImplmentClassInfo)

	SLATE_END_ARGS()


public:

	void Construct(const FArguments& InArgs);

	/** Interpret Escape and Enter key press as Cancel or double-click/Next */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Handler for when cancel is clicked */
	void CancelClicked();

	/** Returns true if Finish is allowed */
	bool CanFinish() const;

	/** Handler for when finish is clicked */
	void FinishClicked();

	/** Closes the window that contains this widget */
	void CloseContainingWindow();

	void CreateSourceCode(const FString& InNewClassName, const FString& InNewClassPath, const FModuleContextInfo& InSelectModule, const FNewClassInfo& InParentClassInfo);

private:

	/** Periodic checks for validity will not occur while this flag is true. Used to prevent a frame of "this project already exists" while exiting after a successful creation. */
	bool bPreventPeriodicValidityChecksUntilNextChange = false;

	TSharedPtr< SWizard> MainWizard;

	/** Event called when code is succesfully added to the project */
	FOnAddedToProject OnAddedToProject;


	TWeakObjectPtr<UWidgetBlueprint> WeakWidgetBlueprint;

	TSharedPtr<FWidgetGenClassInfomation> BaseClassInfo;
	TSharedPtr<FWidgetGenClassInfomation> ImplmentClassInfo;

};
