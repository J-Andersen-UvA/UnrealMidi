#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UnrealMidiSubsystem.h"

struct FToucanDeviceRow
{
    FString Name;
    bool bIsInput = true;
    int32 Index = 0;
    bool bSelected = false;
};
using FToucanDeviceRowPtr = TSharedPtr<FToucanDeviceRow>;

class SToucanMidiPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SToucanMidiPicker) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    void RefreshDevices();
    TSharedRef<ITableRow> OnGenerateRow(FToucanDeviceRowPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnToggleDevice(ECheckBoxState NewState, FToucanDeviceRowPtr Item);
    FReply OnSaveSelection();
    FReply OnRefreshClicked();
    
    // Banner helpers
    void UpdateSavedSelectionStatus();
    EVisibility GetMissingBannerVisibility() const;
    FText GetMissingBannerText() const;

private:
    TArray<FToucanDeviceRowPtr> Rows;
    TSharedPtr<SListView<FToucanDeviceRowPtr>> ListView;
    
    // Resolved saved selection (from config), with Index == -1 for missing
    TArray<FUnrealMidiDeviceInfo> SavedResolved;
    int32 NumMissing = 0;
    FString MissingSummary;  // cached, pretty list of missing names

    void OpenDeviceSettingsDialog(const FString& DeviceName);
    TOptional<float> TEnterUI, TActiveUI, DebounceUI, IdleUI;

    void OnRowCheckedChanged(ECheckBoxState NewState, FToucanDeviceRowPtr Item);
    bool bDebugPrintUI = false;
};
