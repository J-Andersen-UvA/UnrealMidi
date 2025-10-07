#include "SToucanMidiPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "UnrealMidiSubsystem.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "SToucanMidiPicker"

void SToucanMidiPicker::Construct(const FArguments& Args)
{
    RefreshDevices();

    ChildSlot
    [
        SNew(SBorder)
        .Padding(8)
        [
            SNew(SVerticalBox)

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
    return SNew(STableRow<FToucanDeviceRowPtr>, OwnerTable)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0)
        [
            SNew(SCheckBox)
            .IsChecked_Lambda([Item]{ return Item->bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged(this, &SToucanMidiPicker::OnToggleDevice, Item)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text_Lambda([Item]{
                return FText::FromString(FString::Printf(TEXT("[%s] %d  %s"),
                    Item->bIsInput ? TEXT("IN") : TEXT("OUT"),
                    Item->Index, *Item->Name));
            })
        ]
    ];
}

void SToucanMidiPicker::OnToggleDevice(ECheckBoxState NewState, FToucanDeviceRowPtr Item)
{
    Item->bSelected = (NewState == ECheckBoxState::Checked);
}

FReply SToucanMidiPicker::OnRefreshClicked()
{
    RefreshDevices();
    if (ListView.IsValid()) ListView->RequestListRefresh();
    return FReply::Handled();
}

void SToucanMidiPicker::RefreshDevices()
{
    Rows.Reset();

    if (!GEngine) return;
    if (UUnrealMidiSubsystem* Sys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        TArray<FUnrealMidiDeviceInfo> Devices;
        Sys->EnumerateDevices(Devices);

        for (const auto& D : Devices)
        {
            auto Row = MakeShared<FToucanDeviceRow>();
            Row->Name = D.Name;
            Row->bIsInput = D.bIsInput;
            Row->Index = D.Index;
            Row->bSelected = false; // TODO: load previous selection
            Rows.Add(Row);
        }
    }
}

FReply SToucanMidiPicker::OnSaveSelection()
{
    // For now, just log – later we can persist to config / developer settings.
    UE_LOG(LogTemp, Display, TEXT("ToucanMidiController: Selected devices:"));
    for (const auto& Row : Rows)
    {
        if (Row->bSelected)
        {
            UE_LOG(LogTemp, Display, TEXT(" - [%s] %d  %s"),
                Row->bIsInput ? TEXT("IN") : TEXT("OUT"),
                Row->Index, *Row->Name);
        }
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
