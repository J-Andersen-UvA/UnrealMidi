#include "SToucanMidiPicker.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/AppStyle.h"
#include "UnrealMidiSubsystem.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "ToucanMidiConfig.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "SToucanMidiPicker"

static FString MakeDeviceId(const FToucanDeviceRow& D)
{
    // Indexes can change—use Type + Name
    return FString::Printf(TEXT("%s|%s"), D.bIsInput ? TEXT("IN") : TEXT("OUT"), *D.Name);
}

void SToucanMidiPicker::Construct(const FArguments& Args)
{
    RefreshDevices();
    UpdateSavedSelectionStatus();

    ChildSlot
    [
        SNew(SBorder)
        .Padding(8)
        [
            SNew(SVerticalBox)

                // ===== Missing devices banner (only visible when something is missing) =====
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.4f, 0.0f, 0.0f, 0.3f))
                        .Visibility(this, &SToucanMidiPicker::GetMissingBannerVisibility)
                        .Padding(6)
                        [
                            SNew(STextBlock)
                                .Text(this, &SToucanMidiPicker::GetMissingBannerText)
                                .WrapTextAt(600.f)
                                .ColorAndOpacity(FLinearColor::Red)
                        ]
                ]


            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Refresh", "Refresh"))
                    .OnClicked(this, &SToucanMidiPicker::OnRefreshClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("SaveSelection", "Save Selection"))
                    .OnClicked(this, &SToucanMidiPicker::OnSaveSelection)
                ]
            ]

            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SAssignNew(ListView, SListView<FToucanDeviceRowPtr>)
                .ListItemsSource(&Rows)
                .OnGenerateRow(this, &SToucanMidiPicker::OnGenerateRow)
                .SelectionMode(ESelectionMode::None)
            ]
        ]
    ];
}

TSharedRef<ITableRow> SToucanMidiPicker::OnGenerateRow(FToucanDeviceRowPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    check(Item.IsValid());

    return SNew(STableRow<FToucanDeviceRowPtr>, OwnerTable)
        [
            SNew(SHorizontalBox)

                // Checkbox (select this device)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(4, 0, 8, 0)
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([Item]()
                            {
                                return Item->bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged(this, &SToucanMidiPicker::OnRowCheckedChanged, Item)
                ]

            // Device label  [IN]/[OUT]  Name  (Index)
            + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text_Lambda([Item]()
                            {
                                return FText::FromString(
                                    FString::Printf(TEXT("[%s] %s  (%d)"),
                                        Item->bIsInput ? TEXT("IN") : TEXT("OUT"),
                                        *Item->Name,
                                        Item->Index));
                            })
                        .HighlightText(FText::GetEmpty())
                ]

            // Gear button → per-device filter settings
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(6, 0, 0, 0)
                [
                    SNew(SButton)
                        .ToolTipText(NSLOCTEXT("ToucanMidi", "EditFilterTip", "Edit per-device filter settings"))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .OnClicked_Lambda([this, DevName = Item->Name]()
                            {
                                OpenDeviceSettingsDialog(DevName);
                                return FReply::Handled();
                            })
                        [
                            SNew(SImage)
                                .Image(FAppStyle::Get().GetBrush("Icons.Settings"))
                        ]
                ]
        ];
}

void SToucanMidiPicker::OnRowCheckedChanged(ECheckBoxState NewState, FToucanDeviceRowPtr Item)
{
    if (!Item.IsValid()) return;
    Item->bSelected = (NewState == ECheckBoxState::Checked);
    // If you want immediate persistence you can call OnSaveSelection() here,
    // otherwise you already have a "Save Selection" button.
}

void SToucanMidiPicker::OnToggleDevice(ECheckBoxState NewState, FToucanDeviceRowPtr Item)
{
    Item->bSelected = (NewState == ECheckBoxState::Checked);
}

FReply SToucanMidiPicker::OnRefreshClicked()
{
    RefreshDevices();
    UpdateSavedSelectionStatus();
    if (ListView.IsValid()) ListView->RequestListRefresh();
    return FReply::Handled();
}

void SToucanMidiPicker::RefreshDevices()
{
    Rows.Reset();

    if (!GEngine) return;

    // 1) Load saved IDs (for pre-checking)
    TArray<FString> SavedIds;
    GConfig->GetArray(ToucanCfg::Section, ToucanCfg::Key, SavedIds, GEditorPerProjectIni);

    if (UUnrealMidiSubsystem* Sys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        // 2) Resolve saved selection vs. live devices (for banner only)
        SavedResolved.Reset();
        Sys->GetSavedMidiControllers(SavedResolved); // Index == -1 for missing

        // 3) Build live rows and pre-check based on SavedIds
        TArray<FUnrealMidiDeviceInfo> Devices;
        Sys->EnumerateDevices(Devices);

        for (const auto& D : Devices)
        {
            auto Row = MakeShared<FToucanDeviceRow>();
            Row->Name = D.Name;
            Row->bIsInput = D.bIsInput;
            Row->Index = D.Index;

            const FString Id = FString::Printf(TEXT("%s|%s"),
                Row->bIsInput ? TEXT("IN") : TEXT("OUT"),
                *Row->Name);
            Row->bSelected = SavedIds.Contains(Id);   // restore checkmarks

            Rows.Add(Row);
        }

        // 4) Update banner state (counts any SavedResolved with Index == -1)
        UpdateSavedSelectionStatus();  // <-- add this call
    }
}

FReply SToucanMidiPicker::OnSaveSelection()
{
    TArray<FString> Ids;
    Ids.Reserve(Rows.Num());

    for (const auto& Row : Rows)
    {
        if (Row->bSelected)
        {
            Ids.Add(MakeDeviceId(*Row));
        }
    }

    // Persist to Editor-per-project user settings
    GConfig->SetArray(ToucanCfg::Section, ToucanCfg::Key, Ids, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);

    // Re-resolve after saving (in case user unchecked/checked stuff)
    if (GEngine)
    {
        if (UUnrealMidiSubsystem* Sys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
        {
            SavedResolved.Reset();
            Sys->GetSavedMidiControllers(SavedResolved);
            UpdateSavedSelectionStatus();
        }
    }

    UE_LOG(LogTemp, Display, TEXT("ToucanMidiController: Saved %d selected device(s)."), Ids.Num());
    return FReply::Handled();
}

void SToucanMidiPicker::UpdateSavedSelectionStatus()
{
    NumMissing = 0;
    MissingSummary.Reset();

    // Count and collect names of missing saved devices
    TArray<FString> MissingNames;
    for (const auto& Saved : SavedResolved)
    {
        if (Saved.Index < 0)
        {
            ++NumMissing;
            MissingNames.Add(
                FString::Printf(TEXT("[%s] %s"),
                    Saved.bIsInput ? TEXT("IN") : TEXT("OUT"),
                    *Saved.Name));
        }
    }

    if (MissingNames.Num() > 0)
    {
        MissingSummary = FString::Join(MissingNames, TEXT(", "));
    }
}

void SToucanMidiPicker::OpenDeviceSettingsDialog(const FString& DeviceName)
{
    FMidiFilterSettings S = {};
    if (GEngine)
    {
        if (auto* Sys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
        {
            // Load current (or defaults)
            Sys->GetDeviceFilterSettings(DeviceName, S);
        }
    }
    TEnterUI = S.TEnter;
    TActiveUI = S.TActive;
    DebounceUI = S.DebounceSeconds;
    IdleUI = S.IdleSeconds;

    TSharedRef<SWindow> Win = SNew(SWindow)
        .Title(FText::Format(LOCTEXT("DeviceFilterTitle", "Filter: {0}"), FText::FromString(DeviceName)))
        .AutoCenter(EAutoCenter::PreferredWorkArea)
        .SupportsMaximize(false).SupportsMinimize(false)
        .ClientSize(FVector2D(420, 220));

    Win->SetContent(
        SNew(SBorder).Padding(8)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    SNew(STextBlock).Text(LOCTEXT("FilterExplain",
                        "Schmitt-trigger hysteresis\n"
                        "TEnter: engage threshold\n"
                        "TActive: while moving\n"
                        "Debounce: ignore tiny changes after events\n"
                        "Idle: disarm after inactivity"))
                ]

            // TEnter
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [SNew(STextBlock).Text(LOCTEXT("TEnter", "TEnter"))]
                        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [SNew(SSlider).MinValue(0.f).MaxValue(0.2f).Value(TEnterUI.Get(0.02f)).OnValueChanged_Lambda([this](float v) { TEnterUI = v; })]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                        [SNew(SNumericEntryBox<float>).MinValue(0.f).MaxValue(1.f).Delta(0.001f).Value(TEnterUI).OnValueChanged_Lambda([this](float v) { TEnterUI = v; })]
                ]

            // TActive
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [SNew(STextBlock).Text(LOCTEXT("TActive", "TActive"))]
                        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [SNew(SSlider).MinValue(0.f).MaxValue(0.05f).Value(TActiveUI.Get(0.004f)).OnValueChanged_Lambda([this](float v) { TActiveUI = v; })]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                        [SNew(SNumericEntryBox<float>).MinValue(0.f).MaxValue(1.f).Delta(0.001f).Value(TActiveUI).OnValueChanged_Lambda([this](float v) { TActiveUI = v; })]
                ]

            // Debounce
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [SNew(STextBlock).Text(LOCTEXT("Debounce", "Debounce (s)"))]
                        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [SNew(SSlider).MinValue(0.f).MaxValue(0.1f).Value(DebounceUI.Get(0.035f)).OnValueChanged_Lambda([this](float v) { DebounceUI = v; })]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                        [SNew(SNumericEntryBox<float>).MinValue(0.f).MaxValue(2.f).Delta(0.005f).Value(DebounceUI).OnValueChanged_Lambda([this](float v) { DebounceUI = v; })]
                ]

            // Idle
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [SNew(STextBlock).Text(LOCTEXT("Idle", "Idle (s)"))]
                        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [SNew(SSlider).MinValue(0.05f).MaxValue(0.5f).Value(IdleUI.Get(0.12f)).OnValueChanged_Lambda([this](float v) { IdleUI = v; })]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                        [SNew(SNumericEntryBox<float>).MinValue(0.f).MaxValue(5.f).Delta(0.01f).Value(IdleUI).OnValueChanged_Lambda([this](float v) { IdleUI = v; })]
                ]

            // Buttons
            + SVerticalBox::Slot().AutoHeight().Padding(0, 10)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("Save", "Save"))
                                .OnClicked_Lambda([this, WinPtr = &Win.Get(), DeviceName]()
                                    {
                                        if (GEngine)
                                            if (auto* Sys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
                                            {
                                                FMidiFilterSettings NewS;
                                                NewS.TEnter = TEnterUI.Get(0.02f);
                                                NewS.TActive = TActiveUI.Get(0.004f);
                                                NewS.DebounceSeconds = DebounceUI.Get(0.035f);
                                                NewS.IdleSeconds = IdleUI.Get(0.12f);
                                                Sys->SetDeviceFilterSettings(DeviceName, NewS);
                                            }
                                        WinPtr->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("Cancel", "Cancel"))
                                .OnClicked_Lambda([WinPtr = &Win.Get()]() { WinPtr->RequestDestroyWindow(); return FReply::Handled(); })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddWindow(Win);
}

EVisibility SToucanMidiPicker::GetMissingBannerVisibility() const
{
    return (NumMissing > 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SToucanMidiPicker::GetMissingBannerText() const
{
    if (NumMissing <= 0)
    {
        return FText::GetEmpty();
    }

    return FText::FromString(
        FString::Printf(TEXT("WARNING\nSome saved MIDI devices are not connected (%d): %s"),
            NumMissing, *MissingSummary));
}

#undef LOCTEXT_NAMESPACE
