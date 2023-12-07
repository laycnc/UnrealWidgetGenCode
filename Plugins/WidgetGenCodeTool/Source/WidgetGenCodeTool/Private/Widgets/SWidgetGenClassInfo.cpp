// Copyright Epic Games, Inc. All Rights Reserved.


#include "Widgets/SWidgetGenClassInfo.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Interfaces/IProjectManager.h"
#include "SourceCodeNavigation.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "IDocumentation.h"
#include "UObject/UObjectHash.h"
#include "TutorialMetaData.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "FeaturedClasses.inl"
#include "Editor.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "ClassIconFinder.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "GameProjectGeneration"

/** The last selected module name. Meant to keep the same module selected after first selection */
FString SWidgetGenClassInfo::LastSelectedModuleName;


struct FParentClassItem
{
	FNewClassInfo ParentClassInfo;

	FParentClassItem(const FNewClassInfo& InParentClassInfo)
		: ParentClassInfo(InParentClassInfo)
	{}
};


static void FindPublicEngineHeaderFiles(TArray<FString>& OutFiles, const FString& Path)
{
	TArray<FString> ModuleDirs;
	IFileManager::Get().FindFiles(ModuleDirs, *(Path / TEXT("*")), false, true);
	for (const FString& ModuleDir : ModuleDirs)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *(Path / ModuleDir / TEXT("Classes")), TEXT("*.h"), true, false, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *(Path / ModuleDir / TEXT("Public")), TEXT("*.h"), true, false, false);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SWidgetGenClassInfo::Construct(const FArguments& InArgs)
{
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
	}

	// If we've been given an initial path that maps to a valid project module, use that as our initial module and path

	if (!InArgs._InitialPath.IsEmpty())
	{
		const FString AbsoluteInitialPath = FPaths::ConvertRelativePathToFull(InArgs._InitialPath);
		for (const auto& AvailableModule : AvailableModules)
		{
			if (AbsoluteInitialPath.StartsWith(AvailableModule->ModuleSourcePath))
			{
				SelectedModuleInfo = AvailableModule;
				NewClassPath = AbsoluteInitialPath;
				break;
			}
		}
	}

	DefaultClassPrefix = InArgs._DefaultClassPrefix;
	DefaultClassName = InArgs._DefaultClassName;

	// If we didn't get given a valid path override (see above), try and automatically work out the best default module
	// If we have a runtime module with the same name as our project, then use that
	// Otherwise, set out default target module as the first runtime module in the list
	if (!SelectedModuleInfo.IsValid())
	{
		const FString ProjectName = FApp::GetProjectName();

		// Find initially selected module based on simple fallback in this order..
		// Previously selected module, main project module, a  runtime module
		TSharedPtr<FModuleContextInfo> ProjectModule;
		TSharedPtr<FModuleContextInfo> RuntimeModule;

		for (const auto& AvailableModule : AvailableModules)
		{
			// Check if this module matches our last used
			if (AvailableModule->ModuleName == LastSelectedModuleName)
			{
				SelectedModuleInfo = AvailableModule;
				break;
			}

			if (AvailableModule->ModuleName == ProjectName)
			{
				ProjectModule = AvailableModule;
			}

			if (AvailableModule->ModuleType == EHostType::Runtime)
			{
				RuntimeModule = AvailableModule;
			}
		}

		if (!SelectedModuleInfo.IsValid())
		{
			if (ProjectModule.IsValid())
			{
				// use the project module we found
				SelectedModuleInfo = ProjectModule;
			}
			else if (RuntimeModule.IsValid())
			{
				// use the first runtime module we found
				SelectedModuleInfo = RuntimeModule;
			}
			else
			{
				// default to just the first module
				SelectedModuleInfo = AvailableModules[0];
			}
		}

		NewClassPath = SelectedModuleInfo->ModuleSourcePath;
	}

	ClassLocation = GameProjectUtils::EClassLocation::UserDefined; // the first call to UpdateInputValidity will set this correctly based on NewClassPath

	ParentClassInfo = FNewClassInfo(InArgs._Class);

	LastPeriodicValidityCheckTime = 0;
	PeriodicValidityCheckFrequency = 4;
	bLastInputValidityCheckSuccessful = true;
	bPreventPeriodicValidityChecksUntilNextChange = false;

	UpdateInputValidity();

	const float EditableTextHeight = 26.0f;

	OnAddedToProject = InArgs._OnAddedToProject;

	ChildSlot
		[
			SNew(SVerticalBox)

				// Title
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f)
				[
					SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
						.Text(this, &SWidgetGenClassInfo::GetNameClassTitle)
						.TransformPolicy(ETextTransformPolicy::ToUpper)
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				.Padding(0.0f, 10.0f)
				[
					SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 5.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ClassNameDescription", "Enter a name for your new class. Class names may only contain alphanumeric characters, and may not contain a space."))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 2.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ClassNameDetails_Native", "When you click the \"Create\" button below, a header (.h) file and a source (.cpp) file will be made using this name."))
						]

						// Name Error label
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 5.0f)
						[
							SNew(SWarningOrErrorBox)
								.MessageStyle(EMessageStyle::Error)
								.Visibility(this, &SWidgetGenClassInfo::GetNameErrorLabelVisibility)
								.Message(this, &SWidgetGenClassInfo::GetNameErrorLabelText)
						]

						// Properties
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
								.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
								.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(0.0f)
										[
											SNew(SGridPanel)
												.FillColumn(1, 1.0f)
												// Class type label

												+ SGridPanel::Slot(0, 0)
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 12.0f, 0.0f)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("ClassTypeLabel", "Class Type"))
												]

												+ SGridPanel::Slot(1, 0)
												.VAlign(VAlign_Center)
												.HAlign(HAlign_Left)
												.Padding(2.0f)
												[
													SNew(SSegmentedControl<GameProjectUtils::EClassLocation>)
														.Visibility(EVisibility::Visible)
														.OnValueChanged(this, &SWidgetGenClassInfo::OnClassLocationChanged)
														.Value(this, &SWidgetGenClassInfo::IsClassLocationActive)
														+ SSegmentedControl<GameProjectUtils::EClassLocation>::Slot(GameProjectUtils::EClassLocation::Public)
														.Text(LOCTEXT("Public", "Public"))
														.ToolTip(LOCTEXT("ClassLocation_Public", "A public class can be included and used inside other modules in addition to the module it resides in"))
														+ SSegmentedControl<GameProjectUtils::EClassLocation>::Slot(GameProjectUtils::EClassLocation::Private)
														.Text(LOCTEXT("Private", "Private"))
														.ToolTip(LOCTEXT("ClassLocation_Private", "A private class can only be included and used within the module it resides in"))
												]
												// Name label
												+ SGridPanel::Slot(0, 1)
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 12.0f, 0.0f)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("NameLabel", "Name"))
												]

												// Name edit box
												+ SGridPanel::Slot(1, 1)
												.Padding(0.0f, 3.0f)
												.VAlign(VAlign_Center)
												[
													SNew(SBox)
														.HeightOverride(EditableTextHeight)
														.AddMetaData<FTutorialMetaData>(TEXT("ClassName"))
														[
															SNew(SHorizontalBox)

																+ SHorizontalBox::Slot()
																.FillWidth(.7f)
																[
																	SAssignNew(ClassNameEditBox, SEditableTextBox)
																		.Text(this, &SWidgetGenClassInfo::OnGetClassNameText)
																		.OnTextChanged(this, &SWidgetGenClassInfo::OnClassNameTextChanged)
																		.OnTextCommitted(this, &SWidgetGenClassInfo::OnClassNameTextCommitted)
																]

																+ SHorizontalBox::Slot()
																.AutoWidth()
																.Padding(6.0f, 0.0f, 0.0f, 0.0f)
																[
																	SAssignNew(AvailableModulesCombo, SComboBox<TSharedPtr<FModuleContextInfo>>)
																		.Visibility(EVisibility::Visible)
																		.ToolTipText(LOCTEXT("ModuleComboToolTip", "Choose the target module for your new class"))
																		.OptionsSource(&AvailableModules)
																		.InitiallySelectedItem(SelectedModuleInfo)
																		.OnSelectionChanged(this, &SWidgetGenClassInfo::SelectedModuleComboBoxSelectionChanged)
																		.OnGenerateWidget(this, &SWidgetGenClassInfo::MakeWidgetForSelectedModuleCombo)
																		[
																			SNew(STextBlock)
																				.Text(this, &SWidgetGenClassInfo::GetSelectedModuleComboText)
																		]
																]
														]
												]

												// Path label
												+ SGridPanel::Slot(0, 2)
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 12.0f, 0.0f)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("PathLabel", "Path"))
												]

												// Path edit box
												+ SGridPanel::Slot(1, 2)
												.Padding(0.0f, 3.0f)
												.VAlign(VAlign_Center)
												[
													SNew(SVerticalBox)

														// Native C++ path
														+ SVerticalBox::Slot()
														.Padding(0.0f)
														.AutoHeight()
														[
															SNew(SBox)
																.Visibility(EVisibility::Visible)
																.HeightOverride(EditableTextHeight)
																.AddMetaData<FTutorialMetaData>(TEXT("Path"))
																[
																	SNew(SHorizontalBox)

																		+ SHorizontalBox::Slot()
																		.FillWidth(1.0f)
																		[
																			SNew(SEditableTextBox)
																				.Text(this, &SWidgetGenClassInfo::OnGetClassPathText)
																				.OnTextChanged(this, &SWidgetGenClassInfo::OnClassPathTextChanged)
																		]

																		+ SHorizontalBox::Slot()
																		.AutoWidth()
																		.Padding(6.0f, 1.0f, 0.0f, 0.0f)
																		[
																			SNew(SButton)
																				.VAlign(VAlign_Center)
																				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																				.OnClicked(this, &SWidgetGenClassInfo::HandleChooseFolderButtonClicked)
																				[
																					SNew(SImage)
																						.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
																						.ColorAndOpacity(FSlateColor::UseForeground())
																				]
																		]
																]
														]
												]

												// Header output label
												+ SGridPanel::Slot(0, 3)
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 12.0f, 0.0f)
												[
													SNew(STextBlock)
														.Visibility(EVisibility::Visible)
														.Text(LOCTEXT("HeaderFileLabel", "Header File"))
												]

												// Header output text
												+ SGridPanel::Slot(1, 3)
												.Padding(0.0f, 3.0f)
												.VAlign(VAlign_Center)
												[
													SNew(SBox)
														.Visibility(EVisibility::Visible)
														.VAlign(VAlign_Center)
														.HeightOverride(EditableTextHeight)
														[
															SNew(STextBlock)
																.Text(this, &SWidgetGenClassInfo::OnGetClassHeaderFileText)
														]
												]

												// Source output label
												+ SGridPanel::Slot(0, 4)
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 12.0f, 0.0f)
												[
													SNew(STextBlock)
														.Visibility(EVisibility::Visible)
														.Text(LOCTEXT("SourceFileLabel", "Source File"))
												]

												// Source output text
												+ SGridPanel::Slot(1, 4)
												.Padding(0.0f, 3.0f)
												.VAlign(VAlign_Center)
												[
													SNew(SBox)
														.Visibility(EVisibility::Visible)
														.VAlign(VAlign_Center)
														.HeightOverride(EditableTextHeight)
														[
															SNew(STextBlock)
																.Text(this, &SWidgetGenClassInfo::OnGetClassSourceFileText)
														]
												]
										]
								]
						]
				]
		];

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SWidgetGenClassInfo::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Every few seconds, the class name/path is checked for validity in case the disk contents changed and the location is now valid or invalid.
	// After class creation, periodic checks are disabled to prevent a brief message indicating that the class you created already exists.
	// This feature is re-enabled if the user did not restart and began editing parameters again.
	if (!bPreventPeriodicValidityChecksUntilNextChange && (InCurrentTime > LastPeriodicValidityCheckTime + PeriodicValidityCheckFrequency))
	{
		UpdateInputValidity();
	}
}

FText SWidgetGenClassInfo::GetSelectedParentClassName() const
{
	return FText::FromString(FString(TEXT("Test")));
	//return ParentClassInfo.IsSet() ? ParentClassInfo.GetClassName() : FText::GetEmpty();
}

FString GetClassHeaderPath(const UClass* Class)
{
	if (Class)
	{
		FString ClassHeaderPath;
		if (FSourceCodeNavigation::FindClassHeaderPath(Class, ClassHeaderPath) && IFileManager::Get().FileSize(*ClassHeaderPath) != INDEX_NONE)
		{
			return ClassHeaderPath;
		}
	}
	return FString();
}

EVisibility SWidgetGenClassInfo::GetNameErrorLabelVisibility() const
{
	return GetNameErrorLabelText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

FText SWidgetGenClassInfo::GetNameErrorLabelText() const
{
	if (!bLastInputValidityCheckSuccessful)
	{
		return LastInputValidityErrorText;
	}

	return FText::GetEmpty();
}

FText SWidgetGenClassInfo::GetNameClassTitle() const
{
	static const FString NoneString = TEXT("None");

	const FText ParentClassName = GetSelectedParentClassName();
	if (!ParentClassName.IsEmpty() && ParentClassName.ToString() != NoneString)
	{
		return FText::Format(LOCTEXT("NameClassTitle", "Name Your New {0}"), ParentClassName);
	}

	return LOCTEXT("NameClassGenericTitle", "Name Your New Class");
}

FText SWidgetGenClassInfo::OnGetClassNameText() const
{
	return FText::FromString(NewClassName);
}

void SWidgetGenClassInfo::OnClassNameTextChanged(const FText& NewText)
{
	NewClassName = NewText.ToString();
	UpdateInputValidity();
}

void SWidgetGenClassInfo::OnClassNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		if (CanFinish())
		{
			FinishClicked();
		}
	}
}

FText SWidgetGenClassInfo::OnGetClassPathText() const
{
	return FText::FromString(NewClassPath);
}

void SWidgetGenClassInfo::OnClassPathTextChanged(const FText& NewText)
{
	NewClassPath = NewText.ToString();

	// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
	for (const auto& AvailableModule : AvailableModules)
	{
		if (NewClassPath.StartsWith(AvailableModule->ModuleSourcePath))
		{
			SelectedModuleInfo = AvailableModule;
			AvailableModulesCombo->SetSelectedItem(SelectedModuleInfo);
			break;
		}
	}

	UpdateInputValidity();
}

FText SWidgetGenClassInfo::OnGetClassHeaderFileText() const
{
	return FText::FromString(CalculatedClassHeaderName);
}

FText SWidgetGenClassInfo::OnGetClassSourceFileText() const
{
	return FText::FromString(CalculatedClassSourceName);
}

bool SWidgetGenClassInfo::CanFinish() const
{
	return bLastInputValidityCheckSuccessful && ParentClassInfo.IsSet() && FSourceCodeNavigation::IsCompilerAvailable();
}

void SWidgetGenClassInfo::FinishClicked()
{
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
}

FReply SWidgetGenClassInfo::HandleChooseFolderButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowWindowHandle = (ParentWindow.IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString FolderName;
		const FString Title = LOCTEXT("NewClassBrowseTitle", "Choose a source location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowWindowHandle,
			Title,
			NewClassPath,
			FolderName
		);

		if (bFolderSelected)
		{
			if (!FolderName.EndsWith(TEXT("/")))
			{
				FolderName += TEXT("/");
			}

			NewClassPath = FolderName;

			// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
			for (const auto& AvailableModule : AvailableModules)
			{
				if (NewClassPath.StartsWith(AvailableModule->ModuleSourcePath))
				{
					SelectedModuleInfo = AvailableModule;
					AvailableModulesCombo->SetSelectedItem(SelectedModuleInfo);
					break;
				}
			}

			UpdateInputValidity();
		}
	}

	return FReply::Handled();
}

FText SWidgetGenClassInfo::GetSelectedModuleComboText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(SelectedModuleInfo->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(SelectedModuleInfo->ModuleType)));
	return FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args);
}

void SWidgetGenClassInfo::SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo)
{
	const FString& OldModulePath = SelectedModuleInfo->ModuleSourcePath;
	const FString& NewModulePath = Value->ModuleSourcePath;

	SelectedModuleInfo = Value;

	// Update the class path to be rooted to the new module location
	const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(NewClassPath) / ""; // Ensure trailing /
	if (AbsoluteClassPath.StartsWith(OldModulePath))
	{
		NewClassPath = AbsoluteClassPath.Replace(*OldModulePath, *NewModulePath);
	}

	UpdateInputValidity();
}

TSharedRef<SWidget> SWidgetGenClassInfo::MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(Value->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(Value->ModuleType)));
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args));
}

GameProjectUtils::EClassLocation SWidgetGenClassInfo::IsClassLocationActive() const
{
	return ClassLocation;
}

void SWidgetGenClassInfo::OnClassLocationChanged(GameProjectUtils::EClassLocation InLocation)
{
	const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(NewClassPath) / ""; // Ensure trailing /

	GameProjectUtils::EClassLocation TmpClassLocation = GameProjectUtils::EClassLocation::UserDefined;
	GameProjectUtils::GetClassLocation(AbsoluteClassPath, *SelectedModuleInfo, TmpClassLocation);

	const FString RootPath = SelectedModuleInfo->ModuleSourcePath;
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /

	// Update the class path to be rooted to the Public or Private folder based on InVisibility
	switch (InLocation)
	{
	case GameProjectUtils::EClassLocation::Public:
		if (AbsoluteClassPath.StartsWith(PrivatePath))
		{
			NewClassPath = AbsoluteClassPath.Replace(*PrivatePath, *PublicPath);
		}
		else if (AbsoluteClassPath.StartsWith(RootPath))
		{
			NewClassPath = AbsoluteClassPath.Replace(*RootPath, *PublicPath);
		}
		else
		{
			NewClassPath = PublicPath;
		}
		break;

	case GameProjectUtils::EClassLocation::Private:
		if (AbsoluteClassPath.StartsWith(PublicPath))
		{
			NewClassPath = AbsoluteClassPath.Replace(*PublicPath, *PrivatePath);
		}
		else if (AbsoluteClassPath.StartsWith(RootPath))
		{
			NewClassPath = AbsoluteClassPath.Replace(*RootPath, *PrivatePath);
		}
		else
		{
			NewClassPath = PrivatePath;
		}
		break;

	default:
		break;
	}

	// Will update ClassVisibility correctly
	UpdateInputValidity();
}

void SWidgetGenClassInfo::UpdateInputValidity()
{
	bLastInputValidityCheckSuccessful = true;

	// Validate the path first since this has the side effect of updating the UI
	bLastInputValidityCheckSuccessful = GameProjectUtils::CalculateSourcePaths(NewClassPath, *SelectedModuleInfo, CalculatedClassHeaderName, CalculatedClassSourceName, &LastInputValidityErrorText);
	//CalculatedClassHeaderName /= ParentClassInfo.GetHeaderFilename(NewClassName);
	//CalculatedClassSourceName /= ParentClassInfo.GetSourceFilename(NewClassName);
	// todo:takeshot

	// If the source paths check as succeeded, check to see if we're using a Public/Private class
	if (bLastInputValidityCheckSuccessful)
	{
		GameProjectUtils::GetClassLocation(NewClassPath, *SelectedModuleInfo, ClassLocation);

		// We only care about the Public and Private folders
		if (ClassLocation != GameProjectUtils::EClassLocation::Public && ClassLocation != GameProjectUtils::EClassLocation::Private)
		{
			ClassLocation = GameProjectUtils::EClassLocation::UserDefined;
		}
	}
	else
	{
		ClassLocation = GameProjectUtils::EClassLocation::UserDefined;
	}

	// Validate the class name only if the path is valid
	if (bLastInputValidityCheckSuccessful)
	{
		const TSet<FString>& DisallowedHeaderNames = FSourceCodeNavigation::GetSourceFileDatabase().GetDisallowedHeaderNames();
		bLastInputValidityCheckSuccessful = GameProjectUtils::IsValidClassNameForCreation(NewClassName, *SelectedModuleInfo, DisallowedHeaderNames, LastInputValidityErrorText);
	}

	// Validate that the class is valid for the currently selected module
	// As a project can have multiple modules, this lets us update the class validity as the user changes the target module
	if (bLastInputValidityCheckSuccessful && ParentClassInfo.BaseClass)
	{
		bLastInputValidityCheckSuccessful = GameProjectUtils::IsValidBaseClassForCreation(ParentClassInfo.BaseClass, *SelectedModuleInfo);
		if (!bLastInputValidityCheckSuccessful)
		{
			LastInputValidityErrorText = FText::Format(
				LOCTEXT("NewClassError_InvalidBaseClassForModule", "{0} cannot be used as a base class in the {1} module. Please make sure that {0} is API exported."),
				FText::FromString(ParentClassInfo.BaseClass->GetName()),
				FText::FromString(SelectedModuleInfo->ModuleName)
			);
		}
	}

	LastPeriodicValidityCheckTime = FSlateApplication::Get().GetCurrentTime();

	// Since this function was invoked, periodic validity checks should be re-enabled if they were disabled.
	bPreventPeriodicValidityChecksUntilNextChange = false;
}

void SWidgetGenClassInfo::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
