#include "MidiInputDevice.h"
#include "HAL/PlatformTime.h"

THIRD_PARTY_INCLUDES_START
#include "RtMidi.h"
THIRD_PARTY_INCLUDES_END

FMidiInputDevice::FMidiInputDevice(const FString& InDeviceName, int32 InPortIndex)
    : DeviceName(InDeviceName), PortIndex(InPortIndex)
{}

FMidiInputDevice::~FMidiInputDevice()
{
    Close();
}

bool FMidiInputDevice::Open()
{
    Close();

    try
    {
        auto* In = new RtMidiIn(RtMidi::Api::WINDOWS_MM);
        In->ignoreTypes(false, false, false);
        In->openPort(PortIndex, TCHAR_TO_UTF8(*FString::Printf(TEXT("UnrealMidi_%s"), *DeviceName)));

        // Keep 'this' + no extra heap allocs
        In->setCallback([](double, std::vector<unsigned char>* Msg, void* UserData)
        {
            auto* Self = static_cast<FMidiInputDevice*>(UserData);
            if (!Self || !Msg || Msg->empty()) return;

            const uint8 s = (*Msg)[0];
            const int Chan = (s & 0x0F) + 1;
            const uint8 status = s & 0xF0;

            const double Now = Self->NowSeconds();

            switch (status)
            {
                case 0xB0: // CC
                    if (Msg->size() >= 3)
                        Self->HandleCc(Chan, (int)(*Msg)[1], (int)(*Msg)[2]);
                    break;
                case 0x90: // Note On (velocity>0) / Off if 0
                    if (Msg->size() >= 3)
                        Self->HandleNote(Chan, (int)(*Msg)[1], (int)(*Msg)[2] > 0);
                    break;
                case 0x80: // Note Off
                    if (Msg->size() >= 2)
                        Self->HandleNote(Chan, (int)(*Msg)[1], false);
                    break;
                case 0xC0: // PC (status Cn, data1 = program)
                    if (Msg->size() >= 2)
                        Self->HandleProgramChange(Chan, (int)(*Msg)[1]);  // program 0..127
                    break;

                default:
                    break;
            }
        }, this);

        RtMidiInPtr = In;
        return true;
    }
    catch (RtMidiError& e)
    {
        UE_LOG(LogTemp, Warning, TEXT("RtMidi open failed for '%s': %s"), *DeviceName, UTF8_TO_TCHAR(e.getMessage().c_str()));
        RtMidiInPtr = nullptr;
        return false;
    }
}

void FMidiInputDevice::Close()
{
    if (RtMidiInPtr)
    {
        auto* In = reinterpret_cast<RtMidiIn*>(RtMidiInPtr);
        In->cancelCallback();
        try { In->closePort(); } catch(...) {}
        delete In;
        RtMidiInPtr = nullptr;
    }
}

double FMidiInputDevice::NowSeconds() const
{
    return FPlatformTime::Seconds();
}

void FMidiInputDevice::HandleCc(int32 Chan, int32 Cc, int32 Val0to127)
{
    const float Now = FPlatformTime::Seconds();
    const float Norm = FMath::Clamp(Val0to127 / 127.f, 0.f, 1.f);
    const FString Id = MakeMidiId(DeviceName, TEXT("CC"), Chan, Cc);

    FMidiControlValue V;
    V.Id = Id; V.Label = MakeMidiLabel(TEXT("CC"), Chan, Cc);
    V.Value = Norm; V.TimeSeconds = Now;

    { FScopeLock _(&ValuesMutex); LatestById.FindOrAdd(Id) = V; }
    OnValueDelegate.Broadcast(V);
}

void FMidiInputDevice::HandleNote(int32 Chan, int32 Note, bool bOn)
{
    const float Now = FPlatformTime::Seconds();
    const FString Id = MakeMidiId(DeviceName, TEXT("NOTE"), Chan, Note);

    FMidiControlValue V;
    V.Id = Id; V.Label = MakeMidiLabel(TEXT("NOTE"), Chan, Note);
    V.Value = bOn ? 1.f : 0.f; V.TimeSeconds = Now;

    { FScopeLock _(&ValuesMutex); LatestById.FindOrAdd(Id) = V; }
    OnValueDelegate.Broadcast(V);
}

void FMidiInputDevice::HandleProgramChange(int Chan, int Program)
{
    FMidiControlValue V;
    V.Id          = FString::Printf(TEXT("IN:%s:PC:%d:%d"), *DeviceName, Chan, Program);
    V.Label       = FString::Printf(TEXT("Program ch%d #%d"), Chan, Program);
    V.Value       = FMath::Clamp(static_cast<float>(Program) / 127.f, 0.f, 1.f); // normalized 0..1
    V.TimeSeconds = NowSeconds();

    OnValueDelegate.Broadcast(V);
}
