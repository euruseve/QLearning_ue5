#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Core/QLearningTypes.h"
#include "NeedsComponent.h"
#include "QLearningComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class QLEARNING_API UQLearningComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UQLearningComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // Map<StateKey, Map<Action, QValue>>
    UPROPERTY(BlueprintReadOnly, Category = "Q-Learning")
    TMap<FString, FActionQValues> QTable;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Q-Learning")
    FQLearningParams Params;
    
    UPROPERTY(BlueprintReadOnly, Category = "Q-Learning")
    FNPCState CurrentState;
    
    UPROPERTY(BlueprintReadOnly, Category = "Q-Learning")
    FNPCState PreviousState;

    UPROPERTY(BlueprintReadOnly, Category = "Q-Learning")
    EActionType PreviousAction;

    UPROPERTY(BlueprintReadOnly, Category = "Q-Learning")
    float AccumulatedReward;
    
    UPROPERTY()
    UNeedsComponent* NeedsComponent;
    
    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    EActionType ChooseAction(const TArray<EActionType>& AvailableActions);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    void UpdateQValue(float Reward);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    float GetQValue(const FNPCState& State, EActionType Action) const;

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    void SetQValue(const FNPCState& State, EActionType Action, float Value);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    float CalculateReward(const FNPCState& OldState, const FNPCState& NewState, bool bDied);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    EActionType GetBestAction(const FNPCState& State, const TArray<EActionType>& AvailableActions) const;

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    void SaveQTable(const FString& Filename);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    void LoadQTable(const FString& Filename);

    UFUNCTION(BlueprintCallable, Category = "Q-Learning")
    void UpdateCurrentState();

private:
    float GetMaxQValue(const FNPCState& State, const TArray<EActionType>& AvailableActions) const;
    void InitializeQValue(const FNPCState& State, EActionType Action);
    TArray<EActionType> GetAllActions() const;
};