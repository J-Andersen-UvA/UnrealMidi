#include "MidiMapperModule.h"
#include "MidiEventRouter.h"
#include "MidiMappingManager.h"
#include "UnrealMidiSubsystem.h"
#include "Modules/ModuleManager.h"
//#include "EditingSessionDelegates.h"

IMPLEMENT_MODULE(FMidiMapperModule, MidiMapper)

// Add this line before StartupModule / ShutdownModule
static UMidiEventRouter* GRouter = nullptr;

void SafeInit()
{
    if (UUnrealMidiSubsystem* MidiSys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        static FDelegateHandle ConnH;
        static FDelegateHandle DiscH;

        if (!ConnH.IsValid())
        {
            ConnH = MidiSys->OnDeviceConnected.AddLambda([](const FString& Device)
            {
                if (UMidiMappingManager* M = UMidiMappingManager::Get())
                {
                    M->Initialize(Device, TEXT("Default"));
                    UE_LOG(LogTemp, Log, TEXT("[UnrealMidi] Connected: initialized mapping for %s"), *Device);
                }
            });
        }

        if (!DiscH.IsValid())
        {
            DiscH = MidiSys->OnDeviceDisconnected.AddLambda([](const FString& Device)
            {
                if (UMidiMappingManager* M = UMidiMappingManager::Get())
                {
                    M->DeactivateDevice(Device);
                    UE_LOG(LogTemp, Log, TEXT("[UnrealMidi] Disconnected: deactivated %s"), *Device);
                }
            });
        }
    }
}


void FMidiMapperModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module started."));
    UMidiMappingManager* Manager = UMidiMappingManager::Get();

    GRouter = NewObject<UMidiEventRouter>();
    GRouter->AddToRoot();
    GRouter->Init(Manager);

    if (GEngine) SafeInit();
    else FCoreDelegates::OnPostEngineInit.AddStatic(&SafeInit);
}

void FMidiMapperModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module shut down."));
}

UMidiEventRouter* FMidiMapperModule::GetRouter()
{
    return GRouter;
}
