#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MidiTypes.h"
#include "MidiMappingManager.generated.h"

// Registered externally callable functions
//DECLARE_DELEGATE_FourParams(FMidiFunction, const FString& /*Device*/, int32 /*Control*/, float /*Value*/, const FString& /*FunctionId*/);
DECLARE_DELEGATE_OneParam(FMidiFunction, const FMidiControlValue&);

USTRUCT()
struct FMidiRegisteredFunction
{
    GENERATED_BODY()

    FString Label;
    FString Id;
    FMidiFunction Callback;
};

USTRUCT(BlueprintType)
struct FMidiMappedAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName TargetControl;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName Modus;
};

USTRUCT(BlueprintType)
struct FMidiDeviceMapping
{
    GENERATED_BODY()
    FString RigName;
    TMap<FString, FMidiMappedAction> ControlMappings;
};

UCLASS()
class MIDIMAPPER_API UMidiMappingManager : public UObject
{
    GENERATED_BODY()

public:
    static UMidiMappingManager* Get();

    void Initialize(const FString& InDeviceName, const FString& InRigName);

    void RegisterMapping(const FString& DeviceName, const FString& ControlKey, const FMidiMappedAction& Action);
    bool GetMapping(const FString& DeviceName, const FString& ControlKey, FMidiMappedAction& OutAction) const;

    void RegisterOrUpdate(const FString& DeviceName, const FString& ControlKey, const FMidiMappedAction& Action);
    bool RemoveMapping(const FString& DeviceName, const FString& ControlKey);

    void SaveMappings();
    void SaveMappings(const FString& InDeviceName, const FString& InRigName,
        const TMap<FString, FMidiMappedAction>& InMappings);

    void LoadMappings(const FString& InDeviceName, const FString& InRigName);
    //const TMap<int32, FMidiMappedAction>& GetAll() const { return ControlMappings; }

    UFUNCTION()
    void DeactivateDevice(const FString& InDeviceName);

    FString GetMappingFilePath(const FString& InDeviceName, const FString& InRigName) const;

    const FMidiDeviceMapping* GetDeviceMapping(const FString& DeviceName) const
    {
        return Mappings.Find(DeviceName);
    }

    void ClearMappings(const FString& InDeviceName)
    {
        if (FMidiDeviceMapping* Dev = Mappings.Find(InDeviceName))
        {
            Dev->ControlMappings.Empty();
            SaveMappings(InDeviceName, Dev->RigName, Dev->ControlMappings);
        }
    }

    void ClearRegisteredFunctions();
    void UnregisterTopic(const FString& TopicPrefix);

    void RegisterFunction(const FString& Label, const FString& Id, FMidiFunction Func);
    const TArray<FMidiRegisteredFunction>& GetRegisteredFunctions() const { return RegisteredFunctions; }

    void TriggerFunction(const FString& Id, const FString& Device, int32 Control, float Value);
    void TriggerFunction(const FString& Id, const FString& Device, int32 Control, float Value, EMidiMessageType Type);

    UFUNCTION(BlueprintCallable, Category="MIDI Mapping")
    void SaveAsConfig(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category="MIDI Mapping")
    void ImportConfig(const FString& FilePath);

    static FString MakeMidiMapKey(EMidiMessageType Type, int32 Number)
    {
        switch (Type)
        {
        case EMidiMessageType::CC:       return FString::Printf(TEXT("CC:%d"), Number);
        case EMidiMessageType::PC:       return FString::Printf(TEXT("PC:%d"), Number);
        case EMidiMessageType::NoteOn:
        case EMidiMessageType::NoteOff:  return FString::Printf(TEXT("NOTE:%d"), Number);
        default:                         return FString::Printf(TEXT("OTHER:%d"), Number);
        }
    }

private:
    //FString DeviceName;
    //FString RigName;
    FString MappingFilePath;

    TArray<FMidiRegisteredFunction> RegisteredFunctions;


    //UPROPERTY()
    //TMap<int32, FMidiMappedAction> ControlMappings;

    UPROPERTY()
    TMap<FString, FMidiDeviceMapping> Mappings;

};
