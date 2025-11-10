#pragma once
#include "Widgets/SCompoundWidget.h"

class SMidiMappingWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMidiMappingWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    static SMidiMappingWindow* GetActiveInstance();
    void RefreshList();
    void RefreshBindings();
    void RefreshFunctions();

    struct FControlRow
    {
        FString ActionName;
        FString TargetControl;
        FString Modus;
        FString BoundControlKey;
        bool bIsLearning = false;

        bool bIsProgramChange = false;
        FString PCMode = TEXT("Continuous"); // Later: "Discrete", "Toggle", etc.
    };
    TArray<TSharedPtr<FControlRow>> Rows;

    FReply OnLearnClicked(TSharedPtr<FControlRow> Row);
    FReply OnUnbindClicked(TSharedPtr<FControlRow> Row);

    const TArray<TSharedPtr<FString>>& GetPCModes() const { return PCModes; }

private:
    FString ActiveDeviceName;
    FString CurrentMappingName;

    TArray<TSharedPtr<FString>> AvailableDevices;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> DeviceCombo;

    TArray<TSharedPtr<FString>> PCModes;

    TSharedPtr<SListView<TSharedPtr<FControlRow>>> MappingListView;

    void SetActiveDevice(const FString& Device);

    TSharedRef<ITableRow> GenerateMappingRow(
        TSharedPtr<FControlRow> InItem,
        const TSharedRef<STableViewBase>& OwnerTable);
    void OnLearnedControl(FString DeviceName, FString ControlKey, TSharedPtr<FControlRow> Row);
    FReply OnForgetAllClicked();

    void SafeCancelLearning();
};

class SMidiMappingRow : public SMultiColumnTableRow<TSharedPtr<SMidiMappingWindow::FControlRow>>
{
public:
    SLATE_BEGIN_ARGS(SMidiMappingRow) {}
        SLATE_ARGUMENT(TSharedPtr<SMidiMappingWindow::FControlRow>, RowItem)
        SLATE_ARGUMENT(TWeakPtr<SMidiMappingWindow>, OwnerWindow)
    SLATE_END_ARGS()

    TSharedPtr<SMidiMappingWindow::FControlRow> RowItem;
    TWeakPtr<SMidiMappingWindow> OwnerWindow;

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
    {
        RowItem = InArgs._RowItem;
        OwnerWindow = InArgs._OwnerWindow;
        SMultiColumnTableRow<TSharedPtr<SMidiMappingWindow::FControlRow>>::Construct(
            FSuperRowType::FArguments(), OwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        const FMargin Padding(5.f, 2.f);

        if (ColumnName == "Control")
        {
            return SNew(SBox).Padding(Padding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(RowItem->ActionName))
                ];
        }
        else if (ColumnName == "Learn")
        {
            auto OwnerPinned = OwnerWindow.Pin();

            return SNew(SBox).Padding(Padding)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 5, 0))
                        [
                            SNew(SButton)
                                .Text(FText::FromString("Learn"))
                                .ButtonColorAndOpacity_Lambda([R = RowItem]()
                                    {
                                        if (R->bIsLearning)
                                        {
                                            const double Time = FPlatformTime::Seconds();
                                            const float Alpha = (FMath::Sin(Time * 4.0) + 1.0f) * 0.5f;
                                            const FLinearColor StartColor = FLinearColor(0.2f, 1.0f, 0.5f);
                                            const FLinearColor EndColor = FLinearColor(0.0f, 0.5f, 0.2f);
                                            return FLinearColor::LerpUsingHSV(StartColor, EndColor, Alpha);
                                        }
                                        return FLinearColor::White;
                                    })
                                .OnClicked_Lambda([W = OwnerWindow, R = RowItem]()
                                    {
                                        if (auto P = W.Pin()) P->OnLearnClicked(R);
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                                .Text(FText::FromString("Forget"))
                                .OnClicked_Lambda([W = OwnerWindow, R = RowItem]()
                                    {
                                        if (auto P = W.Pin()) P->OnUnbindClicked(R);
                                        return FReply::Handled();
                                    })
                        ]
                ];
        }
        else if (ColumnName == "Current Mapping")
        {
            return SNew(SBox).Padding(Padding)
                [
                    SNew(STextBlock)
                        .Text_Lambda([R = RowItem]()
                            {
                                return !R->BoundControlKey.IsEmpty()
                                    ? FText::FromString(FString::Printf(TEXT("MIDI %s"), *R->BoundControlKey))
                                    : FText::FromString("Unmapped");
                            })
                ];
        }

        return SNew(SBox).Padding(Padding)
            [
                SNew(STextBlock).Text(FText::FromString("Unknown"))
            ];
    }
};
