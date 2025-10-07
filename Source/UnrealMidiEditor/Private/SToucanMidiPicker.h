#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

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

private:
    TArray<FToucanDeviceRowPtr> Rows;
    TSharedPtr<SListView<FToucanDeviceRowPtr>> ListView;
};
