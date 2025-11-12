#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../Components/NeedsComponent.h"
#include "../Components/QLearningComponent.h"
#include "../Actors/InteractableObject.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NPCCharacter.generated.h"

UENUM(BlueprintType)
enum class ENPCState : uint8
{
    Idle,
    Deciding,
    MovingToObject,
    Interacting
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDiedSignature, ANPCCharacter*, DeadNPC);

UCLASS()
class QLEARNING_API ANPCCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ANPCCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalActionsPerformed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    ENeedType CauseOfDeath = ENeedType::Hunger;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNeedsComponent* NeedsComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UQLearningComponent* QLearningComponent;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    int32 NPCID = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "NPC")
    int32 Generation = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "NPC")
    float Lifetime = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "NPC")
    ENPCState CurrentState = ENPCState::Idle;
    
    UPROPERTY(BlueprintReadWrite, Category = "NPC")
    TArray<AInteractableObject*> AvailableObjects;
    
    UPROPERTY(BlueprintReadOnly, Category = "NPC")
    AInteractableObject* CurrentTarget = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float DecisionInterval = 2.0f;

    UPROPERTY(BlueprintReadWrite, Category = "NPC")
    bool bShowNeedsDebug = false;

protected:
    UFUNCTION()
    void OnNPCDied();

    UFUNCTION()
    void OnInteractionComplete(AActor* User);

    void MakeDecision();
    void FindAvailableObjects();
    TArray<EActionType> GetAvailableActions();
    AInteractableObject* FindObjectForAction(EActionType Action);
    void MoveToObject(AInteractableObject* Object);
    void StartInteractionWithObject();
    
    UFUNCTION()
    void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

private:
    FTimerHandle DecisionTimerHandle;
    AAIController* AIController;
    
    FNPCState StateBeforeAction;
    EActionType ChosenAction;
};