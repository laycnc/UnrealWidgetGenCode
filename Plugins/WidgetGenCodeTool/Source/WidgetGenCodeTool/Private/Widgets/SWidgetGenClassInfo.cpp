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
#include "Editor.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "ClassIconFinder.h"
#include "SWarningOrErrorBox.h"
#include "WidgetGenCodeProjectUtils.h"

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
	ClassInfomation = InArgs._ClassInfo;

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

		for (const auto& AvailableModule : AvailableModules)
		{
			if (ClassInfomation->ClassModule.ModuleName == AvailableModule->ModuleName)
			{
				SelectedModuleInfo = AvailableModule;
				break;
			}
		}
	}

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

		ClassInfomation->ClassPath = SelectedModuleInfo->ModuleSourcePath;
	}

	ClassLocation = GameProjectUtils::EClassLocation::UserDefined; // the first call to UpdateInputValidity will set this correctly based on NewClassPath

	LastPeriodicValidityCheckTime = 0;
	PeriodicValidityCheckFrequency = 4;
	bLastInputValidityCheckSuccessful = true;
	bPreventPeriodicValidityChecksUntilNextChange = false;

	UpdateInputValidity();

	const float EditableTextHeight = 26.0f;

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
														+ SSegmentedControl<GameProjectUtils::EClassLocation>::Slot(GameProjectUtils::EClassLocation::Classes)
														.Text(LOCTEXT("Classes", "Classes"))
														.ToolTip(LOCTEXT("ClassLocation_Classes", "A Classes class can be included and used inside other modules in addition to the module it resides in"))
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
	return LOCTEXT("NameClassGenericTitle", "Name Your New Class");
}

FText SWidgetGenClassInfo::OnGetClassNameText() const
{
	return FText::FromString(ClassInfomation->ClassName);
}

void SWidgetGenClassInfo::OnClassNameTextChanged(const FText& NewText)
{
	ClassInfomation->ClassName = NewText.ToString();
	UpdateInputValidity();
}

FText SWidgetGenClassInfo::OnGetClassPathText() const
{
	return FText::FromString(ClassInfomation->ClassPath);
}

void SWidgetGenClassInfo::OnClassPathTextChanged(const FText& NewText)
{
	ClassInfomation->ClassPath = NewText.ToString();

	// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
	for (const auto& AvailableModule : AvailableModules)
	{
		if (ClassInfomation->ClassPath.StartsWith(AvailableModule->ModuleSourcePath))
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
	return FText::FromString(ClassInfomation->ClassHeaderPath);
}

FText SWidgetGenClassInfo::OnGetClassSourceFileText() const
{
	return FText::FromString(ClassInfomation->ClassSourcePath);
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
			ClassInfomation->ClassPath,
			FolderName
		);

		if (bFolderSelected)
		{
			if (!FolderName.EndsWith(TEXT("/")))
			{
				FolderName += TEXT("/");
			}

			ClassInfomation->ClassPath = FolderName;

			// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
			for (const auto& AvailableModule : AvailableModules)
			{
				if (ClassInfomation->ClassPath.StartsWith(AvailableModule->ModuleSourcePath))
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
	const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(ClassInfomation->ClassPath) / ""; // Ensure trailing /
	if (AbsoluteClassPath.StartsWith(OldModulePath))
	{
		ClassInfomation->ClassPath = AbsoluteClassPath.Replace(*OldModulePath, *NewModulePath);
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
	const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(ClassInfomation->ClassPath) / ""; // Ensure trailing /

	GameProjectUtils::EClassLocation TmpClassLocation = GameProjectUtils::EClassLocation::UserDefined;
	GameProjectUtils::GetClassLocation(AbsoluteClassPath, *SelectedModuleInfo, TmpClassLocation);

	const FString RootPath = SelectedModuleInfo->ModuleSourcePath;
	const FString ClassesPath = RootPath / "Classes" / "";		// Ensure trailing /
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /

	auto SetNewClassPath = [&](const FString& InNewClassPath, const FString& InOtherPath1, const FString& InOtherPath2)
		{
			if (AbsoluteClassPath.StartsWith(InOtherPath1))
			{
				ClassInfomation->ClassPath = AbsoluteClassPath.Replace(*InOtherPath1, *InNewClassPath);
			}
			else if (AbsoluteClassPath.StartsWith(InOtherPath2))
			{
				ClassInfomation->ClassPath = AbsoluteClassPath.Replace(*InOtherPath2, *InNewClassPath);
			}
			else if (AbsoluteClassPath.StartsWith(RootPath))
			{
				ClassInfomation->ClassPath = AbsoluteClassPath.Replace(*RootPath, *InNewClassPath);
			}
			else
			{
				ClassInfomation->ClassPath = InNewClassPath;
			}
		};

	// Update the class path to be rooted to the Public or Private folder based on InVisibility
	switch (InLocation)
	{
	case GameProjectUtils::EClassLocation::Classes:
		SetNewClassPath(ClassesPath, PrivatePath, PublicPath);
		break;
	case GameProjectUtils::EClassLocation::Public:
		SetNewClassPath(PublicPath, PrivatePath, ClassesPath);
		break;
	case GameProjectUtils::EClassLocation::Private:
		SetNewClassPath(PrivatePath, PublicPath, ClassesPath);
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
	bLastInputValidityCheckSuccessful = GameProjectUtils::CalculateSourcePaths(ClassInfomation->ClassPath, *SelectedModuleInfo, ClassInfomation->ClassHeaderPath, ClassInfomation->ClassSourcePath, &LastInputValidityErrorText);
	ClassInfomation->ClassHeaderPath /= (ClassInfomation->ClassName + TEXT(".h"));
	ClassInfomation->ClassSourcePath /= (ClassInfomation->ClassName + TEXT(".cpp"));

	// If the source paths check as succeeded, check to see if we're using a Public/Private class
	if (bLastInputValidityCheckSuccessful)
	{
		GameProjectUtils::GetClassLocation(ClassInfomation->ClassPath, *SelectedModuleInfo, ClassLocation);

		// We only care about the Public and Private folders
		if (ClassLocation != GameProjectUtils::EClassLocation::Public && ClassLocation != GameProjectUtils::EClassLocation::Private && ClassLocation != GameProjectUtils::EClassLocation::Classes)
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
		bLastInputValidityCheckSuccessful = GameProjectUtils::IsValidClassNameForCreation(ClassInfomation->ClassName, *SelectedModuleInfo, DisallowedHeaderNames, LastInputValidityErrorText);
	}

	LastPeriodicValidityCheckTime = FSlateApplication::Get().GetCurrentTime();
	
	// Since this function was invoked, periodic validity checks should be re-enabled if they were disabled.
	bPreventPeriodicValidityChecksUntilNextChange = false;
}

#undef LOCTEXT_NAMESPACE
