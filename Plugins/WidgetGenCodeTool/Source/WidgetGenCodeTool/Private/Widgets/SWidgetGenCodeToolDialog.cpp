// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetGenCodeToolDialog.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Workflow/SWizard.h"
#include "SWarningOrErrorBox.h"

#include "ModuleDescriptor.h"
#include "GameProjectUtils.h"
#include "TutorialMetaData.h"

#include "Widgets/SWidgetGenClassInfo.h"

#include "Widgets/SModuleSelecter.h"
#include "SourceCodeNavigation.h"

#define LOCTEXT_NAMESPACE "FWidgetGenCodeToolModule"

void SWidgetGenCodeToolDialog::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SNew(SBorder)
				.Padding(18.0f)
				.BorderImage(FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
				[
					SNew(SVerticalBox)
						.AddMetaData<FTutorialMetaData>(TEXT("AddCodeMajorAnchor"))

						+ SVerticalBox::Slot()
						[
							SAssignNew(MainWizard, SWizard)
								.ShowPageList(false)
								.CanFinish(this, &SWidgetGenCodeToolDialog::CanFinish)
								.FinishButtonText(LOCTEXT("FinishButtonText_Native", "Create Class"))
								.FinishButtonToolTip(LOCTEXT("FinishButtonToolTip_Native", "Creates the code files to add your new class."))
								.OnCanceled(this, &SWidgetGenCodeToolDialog::CancelClicked)
								.OnFinished(this, &SWidgetGenCodeToolDialog::FinishClicked)

								// Name class
								+SWizard::Page()
								//	.OnEnter(this, &SWidgetGenCodeToolDialog::OnNamePageEntered)
								[
									SNew(SWidgetGenClassInfo)
								]

								// Name class
								+ SWizard::Page()
								//	.OnEnter(this, &SWidgetGenCodeToolDialog::OnNamePageEntered)
								[
									SNew(SWidgetGenClassInfo)
								]
						]
				]
		];
}

/** Interpret Escape and Enter key press as Cancel or double-click/Next */
FReply SWidgetGenCodeToolDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) 
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Pressing Escape returns as if the user clicked Cancel
		CancelClicked();
		return FReply::Handled();
	}

	return FReply::Unhandled();

}

/** Handler for when cancel is clicked */
void SWidgetGenCodeToolDialog::CancelClicked()
{
	CloseContainingWindow();
}

/** Returns true if Finish is allowed */
bool SWidgetGenCodeToolDialog::CanFinish() const
{
#if 0
	return bLastInputValidityCheckSuccessful && ParentClassInfo.IsSet() && FSourceCodeNavigation::IsCompilerAvailable();
#else
	return false;
#endif
}

/** Handler for when finish is clicked */
void SWidgetGenCodeToolDialog::FinishClicked()
{
#if 0
	check(CanFinish());


	FString HeaderFilePath;
	FString CppFilePath;

	// Track the selected module name so we can default to this next time
	LastSelectedModuleName = SelectedModuleInfo->ModuleName;

	GameProjectUtils::EReloadStatus ReloadStatus;
	FText FailReason;
	const TSet<FString>& DisallowedHeaderNames = FSourceCodeNavigation::GetSourceFileDatabase().GetDisallowedHeaderNames();
	const GameProjectUtils::EAddCodeToProjectResult AddCodeResult = GameProjectUtils::AddCodeToProject(NewClassName, NewClassPath, *SelectedModuleInfo, ParentClassInfo, DisallowedHeaderNames, HeaderFilePath, CppFilePath, FailReason, ReloadStatus);
	if (AddCodeResult == GameProjectUtils::EAddCodeToProjectResult::Succeeded)
	{
		OnAddedToProject.ExecuteIfBound(NewClassName, NewClassPath, SelectedModuleInfo->ModuleName);

		// Reload current project to take into account any new state
		IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath());

		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		// Display a nag if we didn't automatically hot-reload for the newly added class
		bool bWasReloaded = ReloadStatus == GameProjectUtils::EReloadStatus::Reloaded;

		if (bWasReloaded)
		{
			FNotificationInfo Notification(FText::Format(LOCTEXT("AddedClassSuccessNotification", "Added new class {0}"), FText::FromString(NewClassName)));
			FSlateNotificationManager::Get().AddNotification(Notification);
		}

		if (HeaderFilePath.IsEmpty() || CppFilePath.IsEmpty() || !FSlateApplication::Get().SupportsSourceAccess())
		{
			if (!bWasReloaded)
			{
				// Code successfully added, notify the user. We are either running on a platform that does not support source access or a file was not given so don't ask about editing the file
				const FText Message = FText::Format(
					LOCTEXT("AddCodeSuccessWithHotReload", "Successfully added class '{0}', however you must recompile the '{1}' module before it will appear in the Content Browser.")
					, FText::FromString(NewClassName), FText::FromString(SelectedModuleInfo->ModuleName));
				FMessageDialog::Open(EAppMsgType::Ok, Message);
			}
			else
			{
				// Code was added and hot reloaded into the editor, but the user doesn't have a code IDE installed so we can't open the file to edit it now
			}
		}
		else
		{
			bool bEditSourceFilesNow = false;
			if (bWasReloaded)
			{
				// Code was hot reloaded, so always edit the new classes now
				bEditSourceFilesNow = true;
			}
			else
			{
				// Code successfully added, notify the user and ask about opening the IDE now
				const FText Message = FText::Format(
					LOCTEXT("AddCodeSuccessWithHotReloadAndSync", "Successfully added class '{0}', however you must recompile the '{1}' module before it will appear in the Content Browser.\n\nWould you like to edit the code now?")
					, FText::FromString(NewClassName), FText::FromString(SelectedModuleInfo->ModuleName));
				bEditSourceFilesNow = (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes);
			}

			if (bEditSourceFilesNow)
			{
				TArray<FString> SourceFiles;
				SourceFiles.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*HeaderFilePath));
				SourceFiles.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CppFilePath));

				FSourceCodeNavigation::OpenSourceFiles(SourceFiles);
			}
		}

		// Sync the content browser to the new class
		UPackage* const ClassPackage = FindPackage(nullptr, *(FString("/Script/") + SelectedModuleInfo->ModuleName));
		if (ClassPackage)
		{
			UClass* const NewClass = static_cast<UClass*>(FindObjectWithOuter(ClassPackage, UClass::StaticClass(), *NewClassName));
			if (NewClass)
			{
				TArray<UObject*> SyncAssets;
				SyncAssets.Add(NewClass);
				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets(SyncAssets);
			}
		}

		// Successfully created the code and potentially opened the IDE. Close the dialog.
		CloseContainingWindow();
	}
	else if (AddCodeResult == GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload)
	{
		OnAddedToProject.ExecuteIfBound(NewClassName, NewClassPath, SelectedModuleInfo->ModuleName);

		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		// Failed to compile new code
		const FText Message = FText::Format(
			LOCTEXT("AddCodeFailed_HotReloadFailed", "Successfully added class '{0}', however you must recompile the '{1}' module before it will appear in the Content Browser. {2}\n\nWould you like to open the Output Log to see more details?")
			, FText::FromString(NewClassName), FText::FromString(SelectedModuleInfo->ModuleName), FailReason);
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes)
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));
		}

		// We did manage to add the code itself, so we can close the dialog.
		CloseContainingWindow();
	}
	else
	{
		// @todo show fail reason in error label
		// Failed to add code
		const FText Message = FText::Format(LOCTEXT("AddCodeFailed_AddCodeFailed", "Failed to add class '{0}'. {1}"), FText::FromString(NewClassName), FailReason);
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
#endif
}

/** Closes the window that contains this widget */
void SWidgetGenCodeToolDialog::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
