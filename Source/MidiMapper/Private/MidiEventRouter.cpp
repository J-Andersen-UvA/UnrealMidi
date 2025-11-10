#include "MidiEventRouter.h"
#include "UnrealMidiSubsystem.h"
#include "MidiTypes.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

void UMidiEventRouter::Init(UMidiMappingManager* InManager)
{
    Manager = InManager;
    UE_LOG(LogTemp, Log, TEXT("MidiEventRouter initialized"));
    TryBind();
}

void UMidiEventRouter::TryBind()
{
    if (GEngine)
    {
        if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
        {
            Midi->OnMidiValue.RemoveDynamic(this, &UMidiEventRouter::OnMidiValueReceived);
            Midi->OnMidiValue.AddDynamic(this, &UMidiEventRouter::OnMidiValueReceived);
            UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: bound to UnrealMidiSubsystem"));
            return;
        }
    }

    FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UMidiEventRouter::BindAfterEngineInit);
}

void UMidiEventRouter::BindAfterEngineInit()
{
    FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
    TryBind();
}

void UMidiEventRouter::ArmLearnOnce(const FString& InDeviceName)
{
    bLearning = true;
    ActiveLearningDevice = InDeviceName;
}

void UMidiEventRouter::CancelLearning()
{
    bLearning = false;
    bSuppressNext = false;
    ActiveLearningDevice.Empty();
    LastLearnedControl = -1;
    OnLearningCancelled.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("MIDI learning cancelled."));
}

void UMidiEventRouter::OnMidiValueReceived(const FMidiControlValue& Value)
{
    FMidiControlValue LocalValue = Value;  // make editable copy

    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("MidiEventRouter: Manager pointer is null!"));
        return;
    }

    // Parse: IN:<DeviceName>:<TYPE>:<Chan>:<Num>
    int32 ControlID = -1;
    int32 Channel = -1;
    {
        TArray<FString> Parts;
        LocalValue.Id.ParseIntoArray(Parts, TEXT(":"), true);
        if (Parts.Num() >= 5)
        {
            Channel = FCString::Atoi(*Parts[3]);
            ControlID = FCString::Atoi(*Parts[4]);
        }
    }
    LocalValue.Channel = Channel;

    if (ControlID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: couldn't parse control id from %s"), *LocalValue.Id);
        return;
    }

    FString DeviceName;
    if (!UUnrealMidiSubsystem::ParseDeviceFromId(LocalValue.Id, DeviceName))
        return;

    // --- learning path ---
    if (bLearning)
    {
        if (!DeviceName.Equals(ActiveLearningDevice, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Warning, TEXT("Ignoring learn input from %s, waiting for %s"), *DeviceName, *ActiveLearningDevice);
            return; // wrong device
        }

        bLearning = false;

        FString Key;
        if (LocalValue.Type == EMidiMessageType::PC)
        {
            Key = FString::Printf(TEXT("PC:%d:*"), Channel);
        }
        else
        {
            Key = UMidiMappingManager::MakeMidiMapKey(LocalValue.Type, ControlID);
        }

        OnLearn.Broadcast(DeviceName, Key);
        bSuppressNext = (Value.Type != EMidiMessageType::PC); // prevent the immediate next event from firing (lifting from a button)
        LastLearnedControl = ControlID;
        return;
    }

    // --- suppression path ---
    if (bSuppressNext)
    {
        if (ControlID == LastLearnedControl)
        {
            bSuppressNext = false;
            return; // skip one duplicate event
        }
        bSuppressNext = false;
    }

    // --- mapped execution ---
    if (LocalValue.Type == EMidiMessageType::PC)
    {
        const FString ExactKey = UMidiMappingManager::MakeMidiMapKey(LocalValue.Type, ControlID);
        FMidiMappedAction Action;

        // 1. Try exact match first (Discrete)
        if (Manager->GetMapping(DeviceName, ExactKey, Action))
        {
            if (Action.PCMode == EMidiPCType::Continuous)
            {
                UE_LOG(LogTemp, Log, TEXT("MidiEventRouter: exact match Continuous"));
                // Continuous mode from exact mapping (still valid)
                const float Normalized = static_cast<float>(ControlID) / 127.f;
                Manager->TriggerFunction(
                    Action.ActionName.ToString(),
                    DeviceName,
                    ControlID,
                    Normalized,
                    LocalValue.Type
                );
            }
            else // Discrete mode
            {
                UE_LOG(LogTemp, Log, TEXT("MidiEventRouter: exact match Discrete"));
                Manager->TriggerFunction(
                    Action.ActionName.ToString(),
                    DeviceName,
                    ControlID,
                    1.0f,
                    LocalValue.Type
                );
            }
            return;
        }

        // 2. Fallback: check for wildcard continuous mapping (PC:<channel>:*)
        const FString WildcardKey = FString::Printf(TEXT("PC:%d:*"), LocalValue.Channel);
        if (Manager->GetMapping(DeviceName, WildcardKey, Action))
        {
            if (Action.PCMode == EMidiPCType::Continuous)
            {
                UE_LOG(LogTemp, Log, TEXT("MidiEventRouter: fallback mode Continuous"));
                const float Normalized = static_cast<float>(ControlID) / 127.f;
                Manager->TriggerFunction(
                    Action.ActionName.ToString(),
                    DeviceName,
                    ControlID,
                    Normalized,
                    LocalValue.Type
                );
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("MidiEventRouter: fallback mode Discrete"));

                // Treat wildcard discrete mappings as simple triggers
                Manager->TriggerFunction(
                    Action.ActionName.ToString(),
                    DeviceName,
                    ControlID,
                    1.0f,
                    LocalValue.Type
                );
            }
            return;
        }

        // No PC mapping found
        UE_LOG(LogTemp, Log, TEXT("MidiEventRouter: ???"));
        return;
    }
    else
    {
        // existing CC, Note, etc.
        FString Key = UMidiMappingManager::MakeMidiMapKey(LocalValue.Type, ControlID);
        FMidiMappedAction Action;
        if (Manager->GetMapping(DeviceName, Key, Action))
        {
            Manager->TriggerFunction(Action.ActionName.ToString(), DeviceName, LocalValue.ControlId, LocalValue.Value, LocalValue.Type);
        }
    }
}
