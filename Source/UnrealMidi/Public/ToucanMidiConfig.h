#pragma once
#include "CoreMinimal.h"

namespace ToucanCfg
{
	// Per-project section used by both runtime + editor code
	inline constexpr const TCHAR* Section      = TEXT("ToucanMidiController");
	inline constexpr const TCHAR* Key          = TEXT("SelectedDevices");  // "IN|Name" / "OUT|Name"
	inline constexpr const TCHAR* ThresholdKey = TEXT("NoiseThreshold");   // optional legacy/global
	// Helper to build per-device section names
	inline FString DeviceSection(const FString& Dev)
	{
		return FString::Printf(TEXT("ToucanMidiController.Device:%s"), *Dev);
	}
}
