// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddToProjectConfig.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "GameProjectUtils.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Optional.h"
#include "Serialization/Archive.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SWidget;
class UClass;
struct FGeometry;
struct FKeyEvent;
struct FModuleContextInfo;
struct FParentClassItem;
struct FWidgetGenClassInfomation;

enum class EClassDomain : uint8 { Blueprint, Native };

/**
 * A dialog to choose a new class parent and name
 */
class SWidgetGenClassInfo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetGenClassInfo)
		: _ClassInfo()
		{}

		SLATE_ARGUMENT(TSharedPtr<FWidgetGenClassInfomation>, ClassInfo)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/** Gets the visibility of the name error label */
	EVisibility GetNameErrorLabelVisibility() const;

	/** Gets the text to display in the name error label */
	FText GetNameErrorLabelText() const;

	/** Returns the title text for the "name class" page */
	FText GetNameClassTitle() const;

	/** Returns the text in the class name edit box */
	FText OnGetClassNameText() const;

	/** Handler for when the text in the class name edit box has changed */
	void OnClassNameTextChanged(const FText& NewText);

	/** Returns the text in the class path edit box */
	FText OnGetClassPathText() const;

	/** Handler for when the text in the class path edit box has changed */
	void OnClassPathTextChanged(const FText& NewText);

	/** Returns the text for the calculated header file name */
	FText OnGetClassHeaderFileText() const;

	/** Returns the text for the calculated source file name */
	FText OnGetClassSourceFileText() const;

	/** Handler for when the "Choose Folder" button is clicked */
	FReply HandleChooseFolderButtonClicked();

	/** Get the combo box text for the currently selected module */
	FText GetSelectedModuleComboText() const;

	/** Called when the currently selected module is changed */
	void SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo);

	/** Create the widget to use as the combo box entry for the given module info */
	TSharedRef<SWidget> MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value);

private:

	/** Checks to see if the given class location is active based on the current value of NewClassPath */
	GameProjectUtils::EClassLocation IsClassLocationActive() const;

	/** Update the value of NewClassPath so that it uses the given class location */
	void OnClassLocationChanged(GameProjectUtils::EClassLocation InLocation);

	/** Checks the current class name/path for validity and updates cached values accordingly */
	void UpdateInputValidity();

private:

	/** The editable text box to enter the current name */
	TSharedPtr<SEditableTextBox> ClassNameEditBox;

	/** The available modules combo box */
	TSharedPtr<SComboBox<TSharedPtr<FModuleContextInfo>>> AvailableModulesCombo;

	/** The last selected module name. Meant to keep the same module selected after first selection */
	static FString LastSelectedModuleName;

	/** The last time that the class name/path was checked for validity. This is used to throttle I/O requests to a reasonable frequency */
	double LastPeriodicValidityCheckTime;

	/** The frequency in seconds for validity checks while the dialog is idle. Changes to the name/path immediately update the validity. */
	double PeriodicValidityCheckFrequency;

	/** Periodic checks for validity will not occur while this flag is true. Used to prevent a frame of "this project already exists" while exiting after a successful creation. */
	bool bPreventPeriodicValidityChecksUntilNextChange;

	/** The error text from the last validity check */
	FText LastInputValidityErrorText;

	/** True if the last validity check returned that the class name/path is valid for creation */
	bool bLastInputValidityCheckSuccessful;

	/** Whether the class should be created as a Public or Private class */
	GameProjectUtils::EClassLocation ClassLocation;

	/** Information about the currently available modules for this project */
	TArray<TSharedPtr<FModuleContextInfo>> AvailableModules;

	/** Information about the currently selected module; used for class validation */
	TSharedPtr<FModuleContextInfo> SelectedModuleInfo;

	TSharedPtr<FWidgetGenClassInfomation> ClassInfomation;

};
