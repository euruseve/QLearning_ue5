#include "InteractableObject.h"
#include "../Components/NeedsComponent.h"
#include "TimerManager.h"

AInteractableObject::AInteractableObject()
{
    PrimaryActorTick.bCanEverTick = false;
    
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(Root);
    
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(Root);
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AInteractableObject::BeginPlay()
{
    Super::BeginPlay();
    
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AInteractableObject::OnTriggerBeginOverlap);
}

void AInteractableObject::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
                                                AActor* OtherActor,
                                                UPrimitiveComponent* OtherComp, 
                                                int32 OtherBodyIndex,
                                                bool bFromSweep, 
                                                const FHitResult& SweepResult)
{

}

void AInteractableObject::StartInteraction(AActor* User)
{
    if (bIsOccupied || !User)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start interaction - object occupied or no user"));
        return;
    }

    bIsOccupied = true;
    CurrentUser = User;

    UE_LOG(LogTemp, Log, TEXT("%s started using %s"), *User->GetName(), *GetName());
    
    GetWorld()->GetTimerManager().SetTimer(InteractionTimerHandle, this, 
                                          &AInteractableObject::CompleteInteraction,
                                          InteractionDuration, false);
}

void AInteractableObject::CompleteInteraction()
{
    if (!CurrentUser)
    {
        bIsOccupied = false;
        return;
    }
    
    UNeedsComponent* NeedsComp = CurrentUser->FindComponentByClass<UNeedsComponent>();
    if (NeedsComp)
    {
        for (const FNeedModifier& Modifier : NeedModifiers)
        {
            NeedsComp->ModifyNeed(Modifier.NeedType, Modifier.ModifierValue);
            UE_LOG(LogTemp, Log, TEXT("Modified %s's need: %d by %.2f"), 
                   *CurrentUser->GetName(), (int32)Modifier.NeedType, Modifier.ModifierValue);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("%s finished using %s"), *CurrentUser->GetName(), *GetName());
    
    OnInteractionComplete.Broadcast(CurrentUser);
    
    CurrentUser = nullptr;
    bIsOccupied = false;
}

void AInteractableObject::EndInteraction()
{
    if (InteractionTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);
    }
    
    CompleteInteraction();
}