#pragma once
#include "CoreMinimal.h"
#include "MidiTypes.generated.h"

USTRUCT(BlueprintType)
struct UNREALMIDI_API FUnrealMidiDeviceInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsInput = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 0; // -1 if missing
};

USTRUCT(BlueprintType)
struct UNREALMIDI_API FMidiControlValue
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;         // IN:Device:Type:Chan:Num
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Label;      // CC ch1 #74 / Note ch10 #60
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float   Value = 0;  // 0..1 (CC) / 0/1 (Note)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float   TimeSeconds = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMidiValueNative, const FMidiControlValue&);

inline FString MakeMidiId(const FString& Device, const TCHAR* Type, int32 Chan, int32 Num)
{
    return FString::Printf(TEXT("IN:%s:%s:%d:%d"), *Device, Type, Chan, Num);
}
inline FString MakeMidiLabel(const TCHAR* Type, int32 Chan, int32 Num)
{
    return FCString::Stricmp(Type, TEXT("NOTE"))==0
        ? FString::Printf(TEXT("Note ch%d #%d"), Chan, Num)
        : FString::Printf(TEXT("CC ch%d #%d"),   Chan, Num);
}
