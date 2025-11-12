#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/QLearningTypes.h"
#include "Components/BoxComponent.h"
#include "InteractableObject.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionComplete, AActor*, User);

UCLASS()
class QLEARNING_API AInteractableObject : public AActor
{
    GENERATED_BODY()

public:
    AInteractableObject();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    EActionType ActionType;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionDuration = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    TArray<FNeedModifier> NeedModifiers;
    
    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    bool bIsOccupied = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    AActor* CurrentUser = nullptr;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;
    
    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionComplete OnInteractionComplete;
    
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool CanInteract() const { return !bIsOccupied; }

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void StartInteraction(AActor* User);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void EndInteraction();

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    const TArray<FNeedModifier>& GetNeedModifiers() const { return NeedModifiers; }

protected:
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                               bool bFromSweep, const FHitResult& SweepResult);

private:
    FTimerHandle InteractionTimerHandle;
    void CompleteInteraction();
};