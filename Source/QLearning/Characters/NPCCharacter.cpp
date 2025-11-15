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
            UE_LOG(LogTemp, Warning, TEXT("%s: Manually spawned AI Controller"), *GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("%s: FAILED to spawn AI Controller!"), *GetName());
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("%s: AI Controller already exists"), *GetName());
    }

    if (NeedsComponent)
    {
        NeedsComponent->OnNPCDied.AddDynamic(this, &ANPCCharacter::OnNPCDied);
    }

    FindAvailableObjects();

    GetWorld()->GetTimerManager().SetTimer(DecisionTimerHandle, this, 
        &ANPCCharacter::MakeDecision,
        DecisionInterval, true);
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
        UE_LOG(LogTemp, Log, TEXT("%s busy (State: %d), skipping decision"), 
               *GetName(), (int32)CurrentState);
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
        UE_LOG(LogTemp, Log, TEXT("%s: All needs high, idling"), *GetName());
        return;
    }
    
    StateBeforeAction = QLearningComponent->CurrentState;
    
    ChosenAction = QLearningComponent->ChooseAction(AvailableActions);
    UE_LOG(LogTemp, Log, TEXT("%s chose action: %d"), *GetName(), (int32)ChosenAction);
    
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
        UE_LOG(LogTemp, Log, TEXT("%s: No object found for action %d"), 
               *GetName(), (int32)ChosenAction);
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
    UE_LOG(LogTemp, Warning, TEXT("%s: Distance to target: %.2f"), *GetName(), Distance);

    CurrentState = ENPCState::MovingToObject;

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalActor(Object);
    MoveRequest.SetAcceptanceRadius(150.0f);

    FNavPathSharedPtr NavPath;
    FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest, &NavPath);

    if (RequestResult.Code != EPathFollowingRequestResult::RequestSuccessful)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: MoveTo request FAILED!"), *GetName());
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

    UE_LOG(LogTemp, Log, TEXT("%s moving to %s"), *GetName(), *Object->GetName());
}

void ANPCCharacter::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    UE_LOG(LogTemp, Warning, TEXT("%s move completed with result: %d"), 
           *GetName(), (int32)Result);
    
    if (AIController && AIController->GetPathFollowingComponent())
    {
        AIController->GetPathFollowingComponent()->OnRequestFinished.Clear();
    }

    if (CurrentTarget)
    {
        float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        UE_LOG(LogTemp, Warning, TEXT("%s distance to target: %.2f"), 
               *GetName(), Distance);
        
        if (Distance < 300.0f)
        {
            UE_LOG(LogTemp, Log, TEXT("%s reached target!"), *GetName());
            StartInteractionWithObject();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("%s too far from target"), *GetName());
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
        UE_LOG(LogTemp, Warning, TEXT("%s: Object became occupied"), *GetName());
        CurrentState = ENPCState::Idle;
        CurrentTarget = nullptr;
        return;
    }

    CurrentState = ENPCState::Interacting;
    
    CurrentTarget->OnInteractionComplete.AddDynamic(this, &ANPCCharacter::OnInteractionComplete);
    
    CurrentTarget->StartInteraction(this);

    UE_LOG(LogTemp, Log, TEXT("%s started interaction with %s"), 
           *GetName(), *CurrentTarget->GetName());
}

void ANPCCharacter::OnInteractionComplete(AActor* User)
{
    if (User != this)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("%s completed interaction"), *GetName());

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

    UE_LOG(LogTemp, Error, TEXT("%s DIED after %.2f seconds (Generation %d)"), 
           *GetName(), Lifetime, Generation);
    
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