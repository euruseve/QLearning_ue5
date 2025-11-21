#pragma once

#include "CoreMinimal.h"
#include "NeedType.h"
#include "QLearningTypes.generated.h"

UENUM(BlueprintType)
enum class ENeedLevel : uint8
{
    Critical    UMETA(DisplayName = "Critical (0-40%)"),    // 0
    Medium      UMETA(DisplayName = "Medium (40-70%)"),     // 1
    High        UMETA(DisplayName = "High (70-100%)"),      // 2
    
    MAX         UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EActionType : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    UseTelevision   UMETA(DisplayName = "Use Television"),
    UseComputer     UMETA(DisplayName = "Use Computer"),
    UseShower       UMETA(DisplayName = "Use Shower"),
    UseRefrigerator UMETA(DisplayName = "Use Refrigerator"),
    UseToilet       UMETA(DisplayName = "Use Toilet"),
    UseBed          UMETA(DisplayName = "Use Bed"),
    UseSofa         UMETA(DisplayName = "Use Sofa"),
    UsePhone        UMETA(DisplayName = "Use Phone"),
    UseSink         UMETA(DisplayName = "Use Sink"),
    UseBookshelf    UMETA(DisplayName = "Use Bookshelf"),
    UseGym          UMETA(DisplayName = "Use Gym"),
    
    MAX             UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FNPCState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<ENeedType, ENeedLevel> NeedLevels;

    FNPCState()
    {
        for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
        {
            NeedLevels.Add((ENeedType)i, ENeedLevel::Medium);
        }
    }

    FString GetStateKey() const
    {
        FString Key = "";
        
        // Явно ітеруємо в порядку enum
        for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
        {
            ENeedType NeedType = (ENeedType)i;
            ENeedLevel Level = NeedLevels.Contains(NeedType) ? 
                               NeedLevels[NeedType] : ENeedLevel::Medium;
            Key += FString::FromInt((int32)Level);
        }
        
        return Key;
    }

    static ENeedLevel ValueToLevel(float Value)
    {
        if (Value <= 40.0f) return ENeedLevel::Critical;
        if (Value <= 70.0f) return ENeedLevel::Medium;
        return ENeedLevel::High;
    }

    bool operator==(const FNPCState& Other) const
    {
        return GetStateKey() == Other.GetStateKey();
    }
};

USTRUCT(BlueprintType)
struct FQValue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float Value;

    UPROPERTY(BlueprintReadWrite)
    int32 TimesVisited;

    FQValue() : Value(0.0f), TimesVisited(0) {}
    FQValue(float InValue) : Value(InValue), TimesVisited(1) {}
};

USTRUCT(BlueprintType)
struct FActionQValues
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<EActionType, FQValue> ActionValues;

    FActionQValues() {}
};

USTRUCT(BlueprintType)
struct FLogEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float Timestamp;

    UPROPERTY(BlueprintReadWrite)
    int32 NPCID;

    UPROPERTY(BlueprintReadWrite)
    int32 Generation;

    UPROPERTY(BlueprintReadWrite)
    EActionType Action;

    UPROPERTY(BlueprintReadWrite)
    FString StateKey;

    UPROPERTY(BlueprintReadWrite)
    float Reward;

    UPROPERTY(BlueprintReadWrite)
    float Lifetime;

    UPROPERTY(BlueprintReadWrite)
    TMap<ENeedType, float> CurrentNeeds;

    FLogEntry() : Timestamp(0.0f), NPCID(0), Generation(0), Action(EActionType::Idle), 
                  StateKey(""), Reward(0.0f), Lifetime(0.0f) {}
};

USTRUCT(BlueprintType)
struct FQLearningParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    float LearningRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    float DiscountFactor = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    float ExplorationRate = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    float ExplorationDecay = 0.998f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    float MinExplorationRate = 0.05f;
};

USTRUCT(BlueprintType)
struct FNeedModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    ENeedType NeedType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    float ModifierValue;

    FNeedModifier() : NeedType(ENeedType::Hunger), ModifierValue(0.0f) {}
    FNeedModifier(ENeedType Type, float Value) : NeedType(Type), ModifierValue(Value) {}
};