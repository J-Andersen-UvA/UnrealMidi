#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UnrealMidiSubsystem.h"
#include "SToucanMidiPicker.h"

#define LOCTEXT_NAMESPACE "FToucanMidiEditor"

static const FName ToucanTabName("ToucanMidiController");

class FUnrealMidiEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // Register tab spawner
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            ToucanTabName,
            FOnSpawnTab::CreateRaw(this, &FUnrealMidiEditorModule::SpawnToucanTab))
        .SetDisplayName(LOCTEXT("ToucanTabDisplay", "ToucanMidiController"))
        .SetTooltipText(LOCTEXT("ToucanTabTooltip", "Select MIDI devices"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

        // Register ToolMenus callback
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealMidiEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        if (UToolMenus* TM = UToolMenus::TryGet())
        {
            TM->UnregisterOwner(FToolMenuOwner(this));
        }
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToucanTabName);
    }

private:
    TSharedRef<SDockTab> SpawnToucanTab(const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SToucanMidiPicker)
            ];
    }

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);

        if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu"))
        {
            FToolMenuSection& Root = Menu->FindOrAddSection("ToucanMidiRoot");
            Root.AddSubMenu(
                "ToucanMidiMenu",
                LOCTEXT("ToucanMidi", "Toucan MIDI"),
                LOCTEXT("ToucanMidi_Tooltip", "MIDI utilities"),
                FNewToolMenuDelegate::CreateRaw(this, &FUnrealMidiEditorModule::BuildToucanMenu));
        }
    }

    void BuildToucanMenu(UToolMenu* Menu)
    {
        FToolMenuSection& Section =
            Menu->AddSection("ToucanMidiSection", LOCTEXT("ToucanMidiSection", "Toucan MIDI"));

        Section.AddMenuEntry(
            "scanForMidiAndSelect",
            LOCTEXT("ScanForMidi", "scanForMidiAndSelect"),
            LOCTEXT("ScanForMidi_Tip", "Open the MIDI device selector"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FUnrealMidiEditorModule::OpenToucanTab)));
    }

    void OpenToucanTab()
    {
        FGlobalTabmanager::Get()->TryInvokeTab(ToucanTabName);
    }
};

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FUnrealMidiEditorModule, UnrealMidiEditor)
