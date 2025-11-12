#include "NPCCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "../Utils/CSVLogger.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utils/GenerationLogger.h"

ANPCCharacter::ANPCCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    NeedsComponent = CreateDefaultSubobject<UNeedsComponent>(TEXT("NeedsComponent"));
    QLearningComponent = CreateDefaultSubobject<UQLearningComponent>(TEXT("QLearningComponent"));
    
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;
}

void ANPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    AIController = Cast<AAIController>(GetController());
    if (!AIController)
    {
        AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
        
        AIController = GetWorld()->SpawnActor<AAIController>(AAIController::StaticClass());
        if (AIController)
        {
            AIController->Possess(this);
            UE_LOG(LogTemp, Warning, TEXT("NPC %d: Manually spawned AI Controller"), NPCID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("NPC %d: FAILED to spawn AI Controller!"), NPCID);
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("NPC %d: AI Controller already exists"), NPCID);
    }

    if (NeedsComponent)
    {
        NeedsComponent->OnNPCDied.AddDynamic(this, &ANPCCharacter::OnNPCDied);
    }

    FindAvailableObjects();

    GetWorld()->GetTimerManager().SetTimer(DecisionTimerHandle, this, 
        &ANPCCharacter::MakeDecision,
        DecisionInterval, true);

    UE_LOG(LogTemp, Warning, TEXT("=== NPC %d DEBUG ==="), NPCID);
    UE_LOG(LogTemp, Warning, TEXT("AIController: %s"), AIController ? TEXT("OK") : TEXT("NULL"));
    UE_LOG(LogTemp, Warning, TEXT("Available Objects: %d"), AvailableObjects.Num());
}


void ANPCCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (NeedsComponent && NeedsComponent->bIsAlive)
    {
        Lifetime += DeltaTime;
    }
}

void ANPCCharacter::FindAvailableObjects()
{
    AvailableObjects.Empty();
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInteractableObject::StaticClass(), FoundActors);
    
    for (AActor* Actor : FoundActors)
    {
        AInteractableObject* Object = Cast<AInteractableObject>(Actor);
        if (Object)
        {
            AvailableObjects.Add(Object);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Found %d interactable objects"), AvailableObjects.Num());
}

TArray<EActionType> ANPCCharacter::GetAvailableActions()
{
    TArray<EActionType> Actions;
    
    Actions.Add(EActionType::Idle);

    for (AInteractableObject* Object : AvailableObjects)
    {
        if (Object && Object->CanInteract())
        {
            Actions.AddUnique(Object->ActionType);
        }
    }

    return Actions;
}

AInteractableObject* ANPCCharacter::FindObjectForAction(EActionType Action)
{
    AInteractableObject* ClosestObject = nullptr;
    float ClosestDistance = MAX_FLT;

    for (AInteractableObject* Object : AvailableObjects)
    {
        if (Object && Object->ActionType == Action && Object->CanInteract())
        {
            float Distance = FVector::Dist(GetActorLocation(), Object->GetActorLocation());
            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestObject = Object;
            }
        }
    }

    return ClosestObject;
}

void ANPCCharacter::MakeDecision()
{
    if (!NeedsComponent || !NeedsComponent->bIsAlive)
    {
        return;
    }
    
    if (CurrentState == ENPCState::MovingToObject || CurrentState == ENPCState::Interacting)
    {
        UE_LOG(LogTemp, Log, TEXT("NPC %d busy (State: %d), skipping decision"), 
               NPCID, (int32)CurrentState);
        return;  
    }

    bool bAllNeedsHigh = true;
    for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
    {
        if (NeedsComponent->GetNeedValue((ENeedType)i) < 80.0f)
        {
            bAllNeedsHigh = false;
            break;
        }
    }

    TArray<EActionType> AvailableActions = GetAvailableActions();

    if (bAllNeedsHigh)
    {
        ChosenAction = EActionType::Idle;
        CurrentState = ENPCState::Idle;
        UE_LOG(LogTemp, Log, TEXT("NPC %d: All needs high, idling"), NPCID);
        return;
    }
    
    StateBeforeAction = QLearningComponent->CurrentState;
    
    ChosenAction = QLearningComponent->ChooseAction(AvailableActions);
    UE_LOG(LogTemp, Log, TEXT("NPC %d chose action: %d"), NPCID, (int32)ChosenAction);

    if (ChosenAction == EActionType::Idle)
    {
        CurrentState = ENPCState::Idle;
        return;
    }
    
    CurrentTarget = FindObjectForAction(ChosenAction);

    if (CurrentTarget)
    {
        MoveToObject(CurrentTarget);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("NPC %d: No object found for action %d"), 
               NPCID, (int32)ChosenAction);
        CurrentState = ENPCState::Idle;
    }
}

void ANPCCharacter::MoveToObject(AInteractableObject* Object)
{
    if (!AIController || !Object)
    {
        CurrentState = ENPCState::Idle;
        return;
    }

    float Distance = FVector::Dist(GetActorLocation(), Object->GetActorLocation());
    UE_LOG(LogTemp, Warning, TEXT("NPC %d: Distance to target: %.2f"), NPCID, Distance);

    CurrentState = ENPCState::MovingToObject;

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalActor(Object);
    MoveRequest.SetAcceptanceRadius(150.0f);

    FNavPathSharedPtr NavPath;
    FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest, &NavPath);

    if (RequestResult.Code != EPathFollowingRequestResult::RequestSuccessful)
    {
        UE_LOG(LogTemp, Error, TEXT("NPC %d: MoveTo request FAILED!"), NPCID);
        CurrentState = ENPCState::Idle;
        return;
    }
    
    if (AIController->GetPathFollowingComponent())
    {
        AIController->GetPathFollowingComponent()->OnRequestFinished.Clear(); 
        
        AIController->GetPathFollowingComponent()->OnRequestFinished.AddLambda(
            [this](FAIRequestID RequestID, const FPathFollowingResult& Result)
            {
                OnMoveCompleted(RequestID, Result.Code);
            }
        );
    }

    UE_LOG(LogTemp, Log, TEXT("NPC %d moving to %s"), NPCID, *Object->GetName());
}

void ANPCCharacter::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    UE_LOG(LogTemp, Warning, TEXT("NPC %d move completed with result: %d"), NPCID, (int32)Result);
    
    if (AIController && AIController->GetPathFollowingComponent())
    {
        AIController->GetPathFollowingComponent()->OnRequestFinished.Clear();
    }

    if (CurrentTarget)
    {
        float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        UE_LOG(LogTemp, Warning, TEXT("NPC %d distance to target: %.2f"), NPCID, Distance);
        
        if (Distance < 300.0f)
        {
            UE_LOG(LogTemp, Log, TEXT("NPC %d reached target!"), NPCID);
            StartInteractionWithObject();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("NPC %d too far from target"), NPCID);
            CurrentState = ENPCState::Idle;
            CurrentTarget = nullptr;
        }
    }
    else
    {
        CurrentState = ENPCState::Idle;
    }
}

void ANPCCharacter::StartInteractionWithObject()
{
    if (!CurrentTarget)
    {
        CurrentState = ENPCState::Idle;
        return;
    }
    
    if (!CurrentTarget->CanInteract())
    {
        UE_LOG(LogTemp, Warning, TEXT("NPC %d: Object became occupied"), NPCID);
        CurrentState = ENPCState::Idle;
        CurrentTarget = nullptr;
        return;
    }

    CurrentState = ENPCState::Interacting;
    
    CurrentTarget->OnInteractionComplete.AddDynamic(this, &ANPCCharacter::OnInteractionComplete);
    
    CurrentTarget->StartInteraction(this);

    UE_LOG(LogTemp, Log, TEXT("NPC %d started interaction with %s"), NPCID, *CurrentTarget->GetName());
}

void ANPCCharacter::OnInteractionComplete(AActor* User)
{
    if (User != this)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("NPC %d completed interaction"), NPCID);

    TotalActionsPerformed++;
    
    QLearningComponent->UpdateCurrentState();
    
    float Reward = QLearningComponent->CalculateReward(
        StateBeforeAction, 
        QLearningComponent->CurrentState, 
        !NeedsComponent->bIsAlive
    );
    
    QLearningComponent->PreviousState = StateBeforeAction;
    QLearningComponent->PreviousAction = ChosenAction;
    QLearningComponent->UpdateQValue(Reward);
    
    UCSVLogger::LogAction(NPCID, Generation, ChosenAction, StateBeforeAction.GetStateKey(),
                         Reward, Lifetime, NeedsComponent->Needs);
    
    CurrentState = ENPCState::Idle;
    if (CurrentTarget)
    {
        CurrentTarget->OnInteractionComplete.RemoveDynamic(this, &ANPCCharacter::OnInteractionComplete);
        CurrentTarget = nullptr;
    }
}

void ANPCCharacter::OnNPCDied()
{
    for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
    {
        ENeedType Need = (ENeedType)i;
        if (NeedsComponent->GetNeedValue(Need) <= 0.0f)
        {
            CauseOfDeath = Need;
            break;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("NPC %d DIED after %.2f seconds (Generation %d)"), 
           NPCID, Lifetime, Generation);
    
    float DeathPenalty = -1000.0f;
    QLearningComponent->UpdateCurrentState();
    QLearningComponent->PreviousState = StateBeforeAction;
    QLearningComponent->PreviousAction = ChosenAction;
    QLearningComponent->UpdateQValue(DeathPenalty);
    
    UCSVLogger::LogDeath(NPCID, Generation, Lifetime, NeedsComponent->Needs);
    
    FGenerationStats Stats;
    Stats.GenerationNumber = Generation;
    Stats.NPCID = NPCID;
    Stats.Lifetime = Lifetime;
    Stats.CauseOfDeath = CauseOfDeath;
    Stats.TotalActions = TotalActionsPerformed;
    
    float TotalNeeds = 0.0f;
    for (const auto& Need : NeedsComponent->Needs)
    {
        TotalNeeds += Need.Value;
    }
    Stats.AverageNeedLevel = TotalNeeds / (int32)ENeedType::MAX;
    Stats.Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));

    UGenerationLogger::LogGeneration(Stats);
    
    QLearningComponent->SaveQTable("QTable.json");
    
    GetWorld()->GetTimerManager().ClearTimer(DecisionTimerHandle);
    
    SetLifeSpan(2.0f);
}