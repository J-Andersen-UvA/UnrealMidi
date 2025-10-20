#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ToucanMidiBPLib.generated.h"

UCLASS() // no abstract, no minimal api — keep it simple
class UNREALMIDI_API UToucanMidiBLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Pure dummy to prove reflection works
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Toucan|Test",
        DisplayName = "Toucan Test Return 42")
    static int32 ToucanReturnFortyTwo()
    {
        return 42;
    }
};
