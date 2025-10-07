DEFINE_LOG_CATEGORY_STATIC(LogUnrealMidi, Log, All);

class FUnrealMidiModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogUnrealMidi, Display, TEXT("[UnrealMidi] StartupModule ok."));
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogUnrealMidi, Display, TEXT("[UnrealMidi] ShutdownModule."));
    }
};

IMPLEMENT_MODULE(FUnrealMidiModule, UnrealMidi)
