// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class SWizard;
struct FModuleContextInfo;

class SWidgetGenCodeToolDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetGenCodeToolDialog)
		{}

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

private:
	TSharedPtr< SWizard> MainWizard;


};
