#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeedsComponent.h"
#include "HighLevelQLearning.generated.h"

// Високорівневі дії (macro-actions)
UENUM(BlueprintType)
enum class EMacroAction : uint8
{
    SatisfyHunger    UMETA(DisplayName = "Satisfy Hunger"),
    SatisfyBladder   UMETA(DisplayName = "Satisfy Bladder"),
    SatisfyEnergy    UMETA(DisplayName = "Satisfy Energy"),
    SatisfySocial    UMETA(DisplayName = "Satisfy Social"),
    SatisfyHygiene   UMETA(DisplayName = "Satisfy Hygiene"),
    SatisfyFun       UMETA(DisplayName = "Satisfy Fun"),
    MAX              UMETA(Hidden)
};

// Високорівневий стан (тільки рівні потреб)
USTRUCT()
struct FHighLevelState
{
    GENERATED_BODY()
    
    TMap<ENeedType, ENeedLevel> NeedLevels;
    
    FString GetStateKey() const
    {
        FString Key = "";
        for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
        {
            ENeedType NeedType = (ENeedType)i;
            if (NeedLevels.Contains(NeedType))
            {
                Key += FString::FromInt((int32)NeedLevels[NeedType]);
            }
            else
            {
                Key += "1"; // Medium за замовчуванням
            }
        }
        return Key;
    }
};

USTRUCT()
struct FHighLevelQValue
{
    GENERATED_BODY()
    
    UPROPERTY()
    float Value = 0.0f;
    
    UPROPERTY()
    int32 TimesVisited = 0;
    
    FHighLevelQValue() : Value(0.0f), TimesVisited(0) {}
    FHighLevelQValue(float InValue) : Value(InValue), TimesVisited(0) {}
};

USTRUCT()
struct FHighLevelActionValues
{
    GENERATED_BODY()
    
    UPROPERTY()
    TMap<EMacroAction, FHighLevelQValue> ActionValues;
};

USTRUCT()
struct FHLQLearningParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    float LearningRate = 0.4f;
    
    UPROPERTY(EditAnywhere)
    float DiscountFactor = 0.95f;
    
    UPROPERTY(EditAnywhere)
    float ExplorationRate = 0.8f;
    
    UPROPERTY(EditAnywhere)
    float ExplorationDecay = 0.998f;
    
    UPROPERTY(EditAnywhere)
    float MinExplorationRate = 0.1f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class QLEARNING_API UHighLevelQLearning : public UActorComponent
{
    GENERATED_BODY()

public:
    UHighLevelQLearning();
    
    virtual void BeginPlay() override;
    
    EMacroAction ChooseMacroAction(const FHighLevelState& State);
    void UpdateQValue(const FHighLevelState& OldState, EMacroAction Action, 
                     float Reward, const FHighLevelState& NewState);
    
    FHighLevelState GetCurrentState() const;
    float GetQValue(const FHighLevelState& State, EMacroAction Action) const;
    void SetQValue(const FHighLevelState& State, EMacroAction Action, float Value);
    float GetMaxQValue(const FHighLevelState& State) const;
    EMacroAction GetBestAction(const FHighLevelState& State) const;
    
    void SaveQTable(const FString& Filename);
    void LoadQTable(const FString& Filename);
    
    UPROPERTY(EditAnywhere, Category = "Q-Learning")
    FHLQLearningParams Params;
    
    UPROPERTY()
    TMap<FString, FHighLevelActionValues> QTable;
    
    UPROPERTY()
    class UNeedsComponent* NeedsComponent;
    
    UPROPERTY()
    FHighLevelState PreviousState;
    
    UPROPERTY()
    EMacroAction PreviousAction;
};