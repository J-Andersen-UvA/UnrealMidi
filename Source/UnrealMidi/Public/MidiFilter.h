#pragma once
#include "CoreMinimal.h"
#include "MidiTypes.h" // FMidiControlValue
#include "MidiFilter.generated.h"

// Tunables (per device)
USTRUCT(BlueprintType)
struct UNREALMIDI_API FMidiFilterSettings
{
    GENERATED_BODY()

    // How much movement required to "engage" a control from idle (0..1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter")
    float TEnter = 0.02f;     // 2%

    // While active, minimum delta to accept (0..1). Often <= 20% of TEnter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter")
    float TActive = 0.004f;   // 0.4% (20% of 2%)

    // Ignore sub-threshold changes for this long after any event (s).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter")
    float DebounceSeconds = 0.035f; // 35 ms

    // Disarm "active" if no accepted events for this long (s).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter")
    float IdleSeconds = 0.12f;     // 120 ms

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter")
    float SuppressOthersAfterDigital = 0.08f; // seconds (80 ms)
};

// Per-control runtime state (keyed by control Id)
struct UNREALMIDI_API FMidiCtrlState
{
    float  Last = 0.f;
    double LastTime = 0.0;
    bool   bActive = false;
    int8   LastSign = 0;
    double LastAnyEventTime = 0.0;
};

/// Stateless helper implementing Schmitt-trigger hysteresis with micro-debounce.
/// Returns true if this event should pass; updates 'State' accordingly.
/// NOTE: bEdge = values near 0 or 1 always pass.
struct UNREALMIDI_API FMidiSchmittFilter
{
    static bool ShouldPass(const FMidiFilterSettings& S,
                           FMidiCtrlState& State,
                           const FMidiControlValue& V)
    {
        const double Now   = V.TimeSeconds;
        const bool bEdge   = (V.Value <= KINDA_SMALL_NUMBER) || (V.Value >= 1.f - KINDA_SMALL_NUMBER);
        const bool bIdle   = (Now - State.LastTime) > S.IdleSeconds;

        if (bIdle) { State.bActive = false; State.LastSign = 0; }

        const float Delta = V.Value - State.Last;
        const float AbsD  = FMath::Abs(Delta);
        const int8  Sign  = (Delta > 0.f) - (Delta < 0.f);

        // Debounce tiny wiggles immediately after any event
        const bool bDebounceSmall = ((Now - State.LastAnyEventTime) < S.DebounceSeconds) &&
                                    (AbsD < (State.bActive ? S.TActive : S.TEnter));

        const float Th = State.bActive ? S.TActive : S.TEnter;

        bool bPass = false;

        if (bEdge || AbsD > Th)
        {
            // Accept
            State.bActive   = true;
            State.Last      = V.Value;
            State.LastTime  = Now;
            if (Sign != 0) State.LastSign = Sign;
            bPass = true;
        }
        else if (!bDebounceSmall)
        {
            // Optional: accumulate tiny deltas here if you want “eventual pass”
            // (we keep it simple for now and drop)
            bPass = false;
        }

        State.LastAnyEventTime = Now;
        return bPass;
    }
};
