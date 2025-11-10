#include "MidiMappingWindow.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.h"
#include "UnrealMidiSubsystem.h"
#include "MidiMapperModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/MessageDialog.h"

#define GRouter FMidiMapperModule::GetRouter()
static TWeakPtr<SMidiMappingWindow> ActiveWindow;

SMidiMappingWindow* SMidiMappingWindow::GetActiveInstance()
{
    return ActiveWindow.IsValid() ? ActiveWindow.Pin().Get() : nullptr;
}

void SMidiMappingWindow::SafeCancelLearning()
{
    if (UMidiEventRouter* Router = FMidiMapperModule::GetRouter())
    {
        if (IsValid(Router))
        {
            Router->CancelLearning();
        }
    }
}

void SMidiMappingWindow::RefreshList()
{
    if (MappingListView.IsValid())
        MappingListView->RequestListRefresh();
}

void SMidiMappingWindow::Construct(const FArguments& InArgs)
{
    ActiveWindow = SharedThis(this);

    PCModes.Empty();
    PCModes.Add(MakeShared<FString>(TEXT("Continuous")));
    PCModes.Add(MakeShared<FString>(TEXT("Discrete")));

    Rows.Empty();
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        for (const auto& F : M->GetRegisteredFunctions())
        {
            auto Row = MakeShared<FControlRow>();
            Row->ActionName = F.Label;
            Row->TargetControl = F.Id;
            Row->Modus = TEXT("Trigger");
            Rows.Add(Row);
        }
    }

    if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        TArray<FUnrealMidiDeviceInfo> Devices;
        Midi->EnumerateDevices(Devices);
        for (const FUnrealMidiDeviceInfo& D : Devices)
            if (D.bIsInput)
                AvailableDevices.Add(MakeShared<FString>(D.Name));

        if (!AvailableDevices.IsEmpty())
            ActiveDeviceName = *AvailableDevices[0];
    }

    ChildSlot
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(STextBlock)
                        .Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("Current mapping: %s"), *CurrentMappingName)); })
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(FText::FromString("Device:"))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
                        [
                            SAssignNew(DeviceCombo, SComboBox<TSharedPtr<FString>>)
                                .OptionsSource(&AvailableDevices)
                                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
                                    {
                                        SafeCancelLearning();
                                        if (NewItem.IsValid())
                                        {
                                            ActiveDeviceName = *NewItem;
                                            RefreshBindings();
                                            RefreshList();
                                        }
                                    })
                                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                    {
                                        return SNew(STextBlock).Text(FText::FromString(*Item));
                                    })
                                [
                                    SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(ActiveDeviceName); })
                                ]
                        ]
                ]
            + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(5, 0)
                        [
                            SNew(SButton)
                                .Text(FText::FromString("Save As"))
                                .OnClicked_Lambda([]()
                                    {
                                        if (UMidiMappingManager* M = UMidiMappingManager::Get())
                                            M->SaveAsConfig(TEXT("TMP_MidiMappingConfig.json"));
                                        return FReply::Handled();
                                    })
                        ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(5, 0)
                        [
                            SNew(SButton)
                                .Text(FText::FromString("Import"))
                                .OnClicked_Lambda([]()
                                    {
                                        if (UMidiMappingManager* M = UMidiMappingManager::Get())
                                            M->ImportConfig(TEXT("TMP_MidiMappingConfig.json"));
                                        return FReply::Handled();
                                    })
                        ]
                ]
            + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Forget All"))
                        .OnClicked(this, &SMidiMappingWindow::OnForgetAllClicked)
                ]
                + SVerticalBox::Slot().FillHeight(1.0f).Padding(5)
                [
                    SAssignNew(MappingListView, SListView<TSharedPtr<FControlRow>>)
                        .ListItemsSource(&Rows)
                        .SelectionMode(ESelectionMode::None)
                        .OnGenerateRow(this, &SMidiMappingWindow::GenerateMappingRow)
                        .HeaderRow(
                            SNew(SHeaderRow)
                            + SHeaderRow::Column("Control").DefaultLabel(FText::FromString("Control")).FillWidth(0.4f)
                            + SHeaderRow::Column("Learn").DefaultLabel(FText::FromString("Learn")).FillWidth(0.3f)
                            + SHeaderRow::Column("Current Mapping").DefaultLabel(FText::FromString("Current Mapping")).FillWidth(0.3f)
                        )
                ]
        ];

    RefreshBindings();
    RefreshList();

    if (UMidiEventRouter* Router = FMidiMapperModule::GetRouter())
    {
        if (IsValid(Router))
        {
            Router->OnLearningCancelled.AddLambda([WeakThis = StaticCastSharedRef<SMidiMappingWindow>(AsShared()).ToWeakPtr()]()
                {
                    if (auto Pinned = WeakThis.Pin())
                    {
                        for (auto& Row : Pinned->Rows)
                            Row->bIsLearning = false;
                        Pinned->RefreshList();
                    }
                });
        }
    }
}

void SMidiMappingWindow::SetActiveDevice(const FString& Device)
{
    ActiveDeviceName = Device;
    RefreshBindings();
}

void SMidiMappingWindow::RefreshFunctions()
{
    Rows.Empty();
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        for (const auto& F : M->GetRegisteredFunctions())
        {
            auto Row = MakeShared<FControlRow>();
            Row->ActionName = F.Label;
            Row->TargetControl = F.Id;
            Row->Modus = TEXT("Trigger");
            Rows.Add(Row);
        }
    }
    RefreshList();
}

void SMidiMappingWindow::RefreshBindings()
{
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        const FMidiDeviceMapping* DeviceMap = M->GetDeviceMapping(ActiveDeviceName);
        if (!DeviceMap) return;

        for (auto& Row : Rows)
        {
            Row->BoundControlKey;
            for (const auto& KVP : DeviceMap->ControlMappings)
            {
                const FMidiMappedAction& Action = KVP.Value;
                if (Action.ActionName.ToString() == Row->ActionName &&
                    Action.TargetControl.ToString() == Row->TargetControl)
                {
                    Row->BoundControlKey = KVP.Key;
                    break;
                }
            }
        }
    }
}

FReply SMidiMappingWindow::OnLearnClicked(TSharedPtr<FControlRow> Row)
{
    if (!Row.IsValid()) return FReply::Handled();

    SafeCancelLearning();
    for (auto& R : Rows) R->bIsLearning = false;
    Row->bIsLearning = true;
    RefreshList();

    UE_LOG(LogTemp, Log, TEXT("Learning for %s"), *Row->ActionName);

    if (UMidiEventRouter* Router = FMidiMapperModule::GetRouter())
    {
        if (IsValid(Router))
        {
            Router->OnMidiLearn().AddSP(this, &SMidiMappingWindow::OnLearnedControl, Row);
            Router->ArmLearnOnce(ActiveDeviceName);
        }
    }
    return FReply::Handled();
}

void SMidiMappingWindow::OnLearnedControl(FString DeviceName, FString ControlKey, TSharedPtr<FControlRow> Row)
{
    if (!Row.IsValid()) return;

    Row->bIsLearning = false;
    Row->BoundControlKey = ControlKey;
    Row->bIsProgramChange = ControlKey.Contains(TEXT("PC:"));

    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        FMidiMappedAction A;
        A.ActionName = FName(*Row->ActionName);
        A.TargetControl = FName(*Row->TargetControl);
        A.Modus = FName(*Row->Modus);
        A.PCMode = Row->bIsProgramChange
            ? (Row->PCMode == "Continuous" ? EMidiPCType::Continuous : EMidiPCType::Discrete)
            : EMidiPCType::Discrete;
        M->RegisterOrUpdate(ActiveDeviceName, ControlKey, A);
    }

    RefreshBindings();
    RefreshList();

    if (UMidiEventRouter* Router = FMidiMapperModule::GetRouter())
        if (IsValid(Router))
            Router->OnMidiLearn().RemoveAll(this);
}

FReply SMidiMappingWindow::OnUnbindClicked(TSharedPtr<FControlRow> Row)
{
    SafeCancelLearning();
    if (!Row.IsValid()) return FReply::Handled();

    if (!Row->BoundControlKey.IsEmpty())
    {
        if (UMidiMappingManager* M = UMidiMappingManager::Get())
            M->RemoveMapping(ActiveDeviceName, Row->BoundControlKey);

        Row->BoundControlKey = TEXT("");
        RefreshBindings();
    }
    return FReply::Handled();
}

FReply SMidiMappingWindow::OnForgetAllClicked()
{
    if (FMessageDialog::Open(EAppMsgType::YesNo,
        FText::FromString(TEXT("Clear all mappings for this device?"))) != EAppReturnType::Yes)
        return FReply::Handled();

    SafeCancelLearning();
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
        M->ClearMappings(ActiveDeviceName);

    for (auto& Row : Rows)
        Row->BoundControlKey = TEXT("");

    RefreshList();
    return FReply::Handled();
}

TSharedRef<ITableRow> SMidiMappingWindow::GenerateMappingRow(
    TSharedPtr<FControlRow> InItem,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SMidiMappingRow, OwnerTable)
        .RowItem(InItem)
        .OwnerWindow(SharedThis(this));
}
