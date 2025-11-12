#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Core/NeedType.h"
#include "../Core/QLearningTypes.h"
#include "NeedsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNeedChanged, ENeedType, NeedType, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDied);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class QLEARNING_API UNeedsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeedsComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    TMap<ENeedType, float> Needs;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    float DegradationRate = 1.0f;
    
    UPROPERTY(BlueprintAssignable, Category = "Needs")
    FOnNeedChanged OnNeedChanged;

    UPROPERTY(BlueprintAssignable, Category = "Needs")
    FOnNPCDied OnNPCDied;
    
    UPROPERTY(BlueprintReadOnly, Category = "Needs")
    bool bIsAlive = true;
    
    UFUNCTION(BlueprintCallable, Category = "Needs")
    void ModifyNeed(ENeedType NeedType, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Needs")
    float GetNeedValue(ENeedType NeedType) const;

    UFUNCTION(BlueprintCallable, Category = "Needs")
    FNPCState GetCurrentState() const;

    UFUNCTION(BlueprintCallable, Category = "Needs")
    void InitializeNeeds(float MinValue = 70.0f, float MaxValue = 80.0f);

    UFUNCTION(BlueprintCallable, Category = "Needs")
    bool IsNeedCritical(ENeedType NeedType) const;

    UFUNCTION(BlueprintCallable, Category = "Needs")
    ENeedType GetMostCriticalNeed() const;

private:
    void DegradeNeeds(float DeltaTime);
    void CheckForDeath();
};