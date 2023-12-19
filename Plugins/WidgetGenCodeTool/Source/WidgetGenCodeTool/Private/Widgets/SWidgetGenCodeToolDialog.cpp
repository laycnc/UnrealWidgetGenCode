// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetGenCodeToolDialog.h"
#include "Widgets/Workflow/SWizard.h"

#include "ModuleDescriptor.h"
#include "TutorialMetaData.h"

#include "Widgets/SWidgetGenClassInfo.h"

#include "GameProjectUtils.h"
#include "ContentBrowserModule.h"
#include "SourceCodeNavigation.h"
#include "Widgets/SModuleSelecter.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IProjectManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "WidgetBlueprint.h"
#include "WidgetGenCodeProjectUtils.h"

#define LOCTEXT_NAMESPACE "FWidgetGenCodeToolModule"

void SWidgetGenCodeToolDialog::Construct(const FArguments& InArgs)
{
	OnAddedToProject = InArgs._OnAddedToProject;
	WeakWidgetBlueprint = InArgs._WidgetBlueprint;
	BaseClassInfo = MakeShared<FWidgetGenClassInfomation>(InArgs._BaseClassInfo);
	ImplmentClassInfo = MakeShared<FWidgetGenClassInfomation>(InArgs._ImplmentClassInfo);

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
								+ SWizard::Page()
								//	.OnEnter(this, &SWidgetGenCodeToolDialog::OnNamePageEntered)
								[
									SNew(SWidgetGenClassInfo)
										.ClassInfo(BaseClassInfo)
								]

								// Name class
								+ SWizard::Page()
								//	.OnEnter(this, &SWidgetGenCodeToolDialog::OnNamePageEntered)
								[
									SNew(SWidgetGenClassInfo)
										.ClassInfo(ImplmentClassInfo)
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
	return true;
#endif
}

/** Handler for when finish is clicked */
void SWidgetGenCodeToolDialog::FinishClicked()
{
	check(CanFinish());

	if (!WeakWidgetBlueprint.IsValid())
	{
		return;
	}

	FScopedSlowTask SlowTask(7, LOCTEXT("AddingCodeToProject", "Adding code to project..."));
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame();

	TArray<FObjectProperty*> Propertys;
	TArray<UClass*> PropertyClasses;
	TArray<FString> PropertyHeaderFiles;

	WidgetGenCodeProjectUtils::GetPropertyInfos(WeakWidgetBlueprint.Get(), Propertys, PropertyClasses, PropertyHeaderFiles);

	// クラスのプロパティをメンバーを初期化
	FString ClassProperties;
	for (const FObjectProperty* Property : Propertys)
	{
		ClassProperties += FString::Printf(TEXT("\tTWeakObjectPtr<U%s> %s;\r\n"),
			*Property->PropertyClass->GetName(),
			*Property->GetName()
		);
	}

	// クラスのプロパティをメンバーを初期化
	FString ClassForwardDeclaration;
	for (const UClass* PropertyClass : PropertyClasses)
	{
		ClassForwardDeclaration += FString::Printf(TEXT("class U%s;\r\n"), *PropertyClass->GetName());
	}

	FString ClassMemberInitialized;
	for (const FObjectProperty* Property : Propertys)
	{
		ClassMemberInitialized += FString::Printf(TEXT("\tSetProperty(TEXT(\"%s\"), %s);\r\n"),
			*Property->GetName(),
			*Property->GetName()
		);
	}

	FString AdditionalIncludeDirectives;
	for (const FString& HeaderFile : PropertyHeaderFiles)
	{
		AdditionalIncludeDirectives += FString::Printf(TEXT("#include \"%s\"\r\n"), *HeaderFile);
	}

#if 1

	SlowTask.EnterProgressFrame();

	TArray<FString> ClassSpecifierList;

	FText FailReason;

	FString SyncHeaderLocation;

	TArray<FString> CreatedFiles;

	if (WidgetGenCodeProjectUtils::GenerateClassHeaderFile(
		*BaseClassInfo,
		FNewClassInfo(WeakWidgetBlueprint->ParentClass),
		ClassSpecifierList,
		ClassProperties,
		ClassForwardDeclaration,
		SyncHeaderLocation,
		FailReason
	))
	{
		CreatedFiles.Add(BaseClassInfo->ClassSourcePath);
	}
	else
	{
		GameProjectUtils::DeleteCreatedFiles(BaseClassInfo->ClassHeaderPath, CreatedFiles);

		// todo 失敗
		return;
	}

	SlowTask.EnterProgressFrame();

	FString SyncSourceLocation;
	if (WidgetGenCodeProjectUtils::GenerateClassSourceFile(
		*BaseClassInfo,
		FNewClassInfo(WeakWidgetBlueprint->ParentClass),
		AdditionalIncludeDirectives,
		ClassMemberInitialized,
		SyncSourceLocation,
		FailReason
	))
	{
		 CreatedFiles.Add(BaseClassInfo->ClassSourcePath);
	}
	else
	{
		GameProjectUtils::DeleteCreatedFiles(BaseClassInfo->ClassSourcePath, CreatedFiles);

		// todo 失敗
		return;
	}

	SlowTask.EnterProgressFrame();

	const bool bProjectHadCodeFiles = GameProjectUtils::ProjectHasCodeFiles();

	FText Failed;
	auto Result = WidgetGenCodeProjectUtils::AddProjectFiles(CreatedFiles, bProjectHadCodeFiles, Failed, &SlowTask);

	if (Result != GameProjectUtils::EAddCodeToProjectResult::Succeeded)
	{
		return;
	}

	GameProjectUtils::EReloadStatus ReloadStatus;
	Result = WidgetGenCodeProjectUtils::ProjectRecompileModule(BaseClassInfo->ClassModule, bProjectHadCodeFiles, ReloadStatus, Failed);

	if (Result != GameProjectUtils::EAddCodeToProjectResult::Succeeded)
	{
		return;
	}

	FSoftClassPath ClassPath(FString::Printf(TEXT("/Script/%s.%s"), *BaseClassInfo->ClassModule.ModuleName, *BaseClassInfo->ClassName));

	UClass* GenBaseClass =	ClassPath.TryLoadClass<UObject>();


	int A = 0;

#endif
}

void SWidgetGenCodeToolDialog::CreateSourceCode(const FString& InNewClassName, const FString& InNewClassPath, const FModuleContextInfo& InSelectModule, const FNewClassInfo& InParentClassInfo)
{
	FString HeaderFilePath;
	FString CppFilePath;

	GameProjectUtils::EReloadStatus ReloadStatus;
	FText FailReason;
	const TSet<FString>& DisallowedHeaderNames = FSourceCodeNavigation::GetSourceFileDatabase().GetDisallowedHeaderNames();
	const GameProjectUtils::EAddCodeToProjectResult AddCodeResult = GameProjectUtils::AddCodeToProject(InNewClassName, InNewClassPath, InSelectModule, InParentClassInfo, DisallowedHeaderNames, HeaderFilePath, CppFilePath, FailReason, ReloadStatus);
	if (AddCodeResult == GameProjectUtils::EAddCodeToProjectResult::Succeeded)
	{
		OnAddedToProject.ExecuteIfBound(InNewClassName, InNewClassPath, InSelectModule.ModuleName);

		// Reload current project to take into account any new state
		IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath());

		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		// Display a nag if we didn't automatically hot-reload for the newly added class
		bool bWasReloaded = ReloadStatus == GameProjectUtils::EReloadStatus::Reloaded;

		if (bWasReloaded)
		{
			FNotificationInfo Notification(FText::Format(LOCTEXT("AddedClassSuccessNotification", "Added new class {0}"), FText::FromString(InNewClassName)));
			FSlateNotificationManager::Get().AddNotification(Notification);
		}

		if (HeaderFilePath.IsEmpty() || CppFilePath.IsEmpty() || !FSlateApplication::Get().SupportsSourceAccess())
		{
			if (!bWasReloaded)
			{
				// Code successfully added, notify the user. We are either running on a platform that does not support source access or a file was not given so don't ask about editing the file
				const FText Message = FText::Format(
					LOCTEXT("AddCodeSuccessWithHotReload", "Successfully added class '{0}', however you must recompile the '{1}' module before it will appear in the Content Browser.")
					, FText::FromString(InNewClassName), FText::FromString(InSelectModule.ModuleName));
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
					, FText::FromString(InNewClassName), FText::FromString(InSelectModule.ModuleName));
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
		UPackage* const ClassPackage = FindPackage(nullptr, *(FString("/Script/") + InSelectModule.ModuleName));
		if (ClassPackage)
		{
			UClass* const NewClass = static_cast<UClass*>(FindObjectWithOuter(ClassPackage, UClass::StaticClass(), *InNewClassName));
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
		OnAddedToProject.ExecuteIfBound(InNewClassName, InNewClassPath, InSelectModule.ModuleName);

		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		// Failed to compile new code
		const FText Message = FText::Format(
			LOCTEXT("AddCodeFailed_HotReloadFailed", "Successfully added class '{0}', however you must recompile the '{1}' module before it will appear in the Content Browser. {2}\n\nWould you like to open the Output Log to see more details?")
			, FText::FromString(InNewClassName), FText::FromString(InSelectModule.ModuleName), FailReason);
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
		const FText Message = FText::Format(LOCTEXT("AddCodeFailed_AddCodeFailed", "Failed to add class '{0}'. {1}"), FText::FromString(InNewClassName), FailReason);
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
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
