#include "MidiMappingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"

UMidiMappingManager* UMidiMappingManager::Get()
{
    static UMidiMappingManager* Singleton = nullptr;
    if (!Singleton)
    {
        Singleton = NewObject<UMidiMappingManager>();
        Singleton->AddToRoot();
    }
    return Singleton;
}

void UMidiMappingManager::Initialize(const FString& InDeviceName, const FString& InRigName)
{
    FString CleanRig = InRigName;

    // Strip suffix of form "X.Y" if identical
    int32 DotPos;
    if (CleanRig.FindChar('.', DotPos))
    {
        FString Left = CleanRig.Left(DotPos);
        FString Right = CleanRig.Mid(DotPos + 1);
        if (Left.Equals(Right, ESearchCase::IgnoreCase))
        {
            CleanRig = Left;
        }
    }

    // Only load once per device; keeps mapping memory persistent.
    if (!Mappings.Contains(InDeviceName))
    {
        FMidiDeviceMapping& DevMap = Mappings.Add(InDeviceName);
        DevMap.RigName = CleanRig;
        LoadMappings(InDeviceName, CleanRig);
    }
}

void UMidiMappingManager::RegisterMapping(const FString& InDeviceName, int32 ControlID, const FMidiMappedAction& Action)
{
    FMidiDeviceMapping& DevMap = Mappings.FindOrAdd(InDeviceName);
    DevMap.ControlMappings.Add(ControlID, Action);
    SaveMappings(InDeviceName, DevMap.RigName, DevMap.ControlMappings);
}

bool UMidiMappingManager::GetMapping(const FString& InDeviceName, int32 ControlID, FMidiMappedAction& OutAction) const
{
    if (const FMidiDeviceMapping* DevMap = Mappings.Find(InDeviceName))
    {
        if (const FMidiMappedAction* Found = DevMap->ControlMappings.Find(ControlID))
        {
            OutAction = *Found;
            return true;
        }
    }
    return false;
}

void UMidiMappingManager::SaveMappings()
{
    // Save all active device maps
    for (const auto& Pair : Mappings)
    {
        const FString& DevName = Pair.Key;
        const FMidiDeviceMapping& Map = Pair.Value;
        SaveMappings(DevName, Map.RigName, Map.ControlMappings);
    }
}

void UMidiMappingManager::SaveMappings(
    const FString& InDeviceName,
    const FString& InRigName,
    const TMap<int32, FMidiMappedAction>& InMappings)
{
    const FString FilePath = GetMappingFilePath(InDeviceName, InRigName);

    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
    for (const auto& Pair : InMappings)
    {
        TSharedPtr<FJsonObject> ActionObj = FJsonObjectConverter::UStructToJsonObject(Pair.Value);
        RootObj->SetObjectField(FString::FromInt(Pair.Key), ActionObj);
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj, Writer);

    FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

void UMidiMappingManager::LoadMappings(const FString& InDeviceName, const FString& InRigName)
{
    FMidiDeviceMapping& DevMap = Mappings.FindOrAdd(InDeviceName);
    DevMap.RigName = InRigName;
    DevMap.ControlMappings.Empty();

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *GetMappingFilePath(InDeviceName, InRigName)))
        return;

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
    {
        for (const auto& Pair : RootObj->Values)
        {
            const TSharedPtr<FJsonObject>* ActionObj;
            if (Pair.Value->TryGetObject(ActionObj))
            {
                FMidiMappedAction Action;
                FJsonObjectConverter::JsonObjectToUStruct(ActionObj->ToSharedRef(), &Action);
                DevMap.ControlMappings.Add(FCString::Atoi(*Pair.Key), Action);
            }
        }
    }
}

FString UMidiMappingManager::GetMappingFilePath(const FString& InDeviceName, const FString& InRigName) const
{
    FString Dir = FPaths::ProjectSavedDir() / TEXT("Config/MidiMappings");
    IFileManager::Get().MakeDirectory(*Dir, true);
    return Dir / FString::Printf(TEXT("%s_%s.json"), *InDeviceName, *InRigName);
}

bool UMidiMappingManager::RemoveMapping(const FString& InDeviceName, int32 ControlID)
{
    if (FMidiDeviceMapping* DevMap = Mappings.Find(InDeviceName))
    {
        const bool bRemoved = DevMap->ControlMappings.Remove(ControlID) > 0;
        if (bRemoved)
        {
            SaveMappings(InDeviceName, DevMap->RigName, DevMap->ControlMappings);
            return true;
        }
    }
    return false;
}

void UMidiMappingManager::RegisterOrUpdate(const FString& InDeviceName, int32 ControlID, const FMidiMappedAction& Action)
{
    RegisterMapping(InDeviceName, ControlID, Action);
}

void UMidiMappingManager::DeactivateDevice(const FString& InDeviceName)
{
    if (FMidiDeviceMapping* Existing = Mappings.Find(InDeviceName))
    {
        SaveMappings(InDeviceName, Existing->RigName, Existing->ControlMappings);
        Mappings.Remove(InDeviceName);
    }
}

void UMidiMappingManager::RegisterFunction(const FString& Label, const FString& Id, FMidiFunction Func)
{
    FMidiRegisteredFunction F;
    F.Label = Label;
    F.Id = Id;
    F.Callback = Func;
    RegisteredFunctions.Add(F);
}

void UMidiMappingManager::TriggerFunction(const FString& Id, const FString& Device, int32 Control, float Value)
{
    for (const auto& F : RegisteredFunctions)
        if (F.Id == Id)
            F.Callback.ExecuteIfBound(Device, Control, Value);
}

void UMidiMappingManager::SaveAsConfig(const FString& FilePath)
{
    FString Path = FilePath;
    if (Path.IsEmpty())
    {
        Path = FPaths::ProjectSavedDir() / TEXT("MIDI/Mappings.json");
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    for (const auto& DevicePair : Mappings)
    {
        const FString& Device = DevicePair.Key;
        const FMidiDeviceMapping& Map = DevicePair.Value;

        TArray<TSharedPtr<FJsonValue>> MappingsArray;
        for (const auto& ControlPair : Map.ControlMappings)
        {
            const int32 ControlId = ControlPair.Key;
            const FMidiMappedAction& Action = ControlPair.Value;

            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("ControlId"), ControlId);
            Entry->SetStringField(TEXT("ActionName"), Action.ActionName.ToString());
            Entry->SetStringField(TEXT("TargetControl"), Action.TargetControl.ToString());
            Entry->SetStringField(TEXT("Modus"), Action.Modus.ToString());
            MappingsArray.Add(MakeShared<FJsonValueObject>(Entry));
        }

        Root->SetArrayField(Device, MappingsArray);
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FFileHelper::SaveStringToFile(Output, *Path);
    UE_LOG(LogTemp, Log, TEXT("Saved MIDI mappings to %s"), *Path);
}

void UMidiMappingManager::ImportConfig(const FString& FilePath)
{
    FString Path = FilePath;
    if (Path.IsEmpty())
    {
        Path = FPaths::ProjectSavedDir() / TEXT("MIDI/Mappings.json");
    }

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("No config found at %s"), *Path);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        return;

    for (const auto& DevicePair : Root->Values)
    {
        const FString Device = DevicePair.Key;
        const TArray<TSharedPtr<FJsonValue>>* DeviceMappingArray;
        if (Root->TryGetArrayField(Device, DeviceMappingArray))
        {
            for (const auto& EntryValue : *DeviceMappingArray)
            {
                const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
                if (!Entry.IsValid()) continue;

                int32 ControlId = Entry->GetIntegerField(TEXT("ControlId"));
                FMidiMappedAction Action;
                Action.ActionName = FName(*Entry->GetStringField(TEXT("ActionName")));
                Action.TargetControl = FName(*Entry->GetStringField(TEXT("TargetControl")));
                Action.Modus = FName(*Entry->GetStringField(TEXT("Modus")));

                RegisterOrUpdate(Device, ControlId, Action);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Imported MIDI mappings from %s"), *Path);
}

void UMidiMappingManager::ClearRegisteredFunctions()
{
    RegisteredFunctions.Empty();
}
