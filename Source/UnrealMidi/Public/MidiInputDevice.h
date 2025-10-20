#pragma once
#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "MidiTypes.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidiSysExNative, const FString& /*DeviceName*/, const TArray<uint8>& /*Bytes*/);

/** Lightweight wrapper around a single RtMidiIn device */
class UNREALMIDI_API FMidiInputDevice
{
public:
    FMidiInputDevice(const FString& InDeviceName, int32 InPortIndex);
    ~FMidiInputDevice();

    bool Open();
    void Close();

    const FString& GetName() const { return DeviceName; }
    int32 GetPortIndex() const { return PortIndex; }

    /** Emits when this device receives a CC/Note change */
    FOnMidiValueNative& OnValue() { return OnValueDelegate; }
    FOnMidiSysExNative& OnSysEx() { return OnSysExDelegate; }

private:
    void HandleCc(int32 Chan, int32 Cc, int32 Val0to127);
    void HandleNote(int32 Chan, int32 Note, bool bOn);

private:
    FString DeviceName;
    int32   PortIndex = -1;

    // Opaque RtMidiIn* stored as void* to keep header clean
    void* RtMidiInPtr = nullptr;
    
    void HandleProgramChange(int Chan, int Program);
    double NowSeconds() const;

    // Per-device latest values (optional; handy if you want to query per-device later)
    FCriticalSection ValuesMutex;
    TMap<FString, FMidiControlValue> LatestById;

    FOnMidiValueNative OnValueDelegate;
    FOnMidiSysExNative OnSysExDelegate;
    void HandleSysEx(const TArray<uint8>& Bytes);
};
