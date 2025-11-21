#include "Characters/NPCCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "../Utils/CSVLogger.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utils/GenerationLogger.h"

ANPCCharacter::ANPCCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    NeedsComponent = CreateDefaultSubobject<UNeedsComponent>(TEXT("NeedsComponent"));
    
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;
}

void ANPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Створюємо High-Level Q-Learning
    HighLevelQL = NewObject<UHighLevelQLearning>(this, UHighLevelQLearning::StaticClass());
    HighLevelQL->RegisterComponent();
    
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
    
    bExecutingMacroAction = false;

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

TArray<AActor*> ANPCCharacter::GetInteractableObjectsForAction(EActionType Action)
{
    TArray<AActor*> Result;
    
    for (AInteractableObject* Object : AvailableObjects)
    {
        if (Object && Object->ActionType == Action && Object->CanInteract())
        {
            Result.Add(Object);
        }
    }
    
    // Сортуємо за відстанню
    Result.Sort([this](const AActor& A, const AActor& B)
    {
        float DistA = FVector::Dist(GetActorLocation(), A.GetActorLocation());
        float DistB = FVector::Dist(GetActorLocation(), B.GetActorLocation());
        return DistA < DistB;
    });
    
    return Result;
}

void ANPCCharacter::MakeDecision()
{
    if (!NeedsComponent || !NeedsComponent->bIsAlive || !HighLevelQL)
    {
        return;
    }
    
    if (bExecutingMacroAction)
    {
        return;
    }
    
    // Safety Net: екстрена логіка для критичних потреб
    ENeedType CriticalNeed = ENeedType::MAX;
    float LowestValue = 100.0f;
    
    for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
    {
        ENeedType NeedType = (ENeedType)i;
        float Value = NeedsComponent->GetNeedValue(NeedType);
        
        if (Value < 25.0f && Value < LowestValue)
        {
            LowestValue = Value;
            CriticalNeed = NeedType;
        }
    }
    
    if (CriticalNeed != ENeedType::MAX)
    {
        UE_LOG(LogTemp, Error, TEXT("🚨 %s EMERGENCY: Need %d = %.1f"), 
               *GetName(), (int32)CriticalNeed, LowestValue);
        ExecuteMacroAction((EMacroAction)((int32)CriticalNeed));
        return;
    }
    
    // High-Level Q-Learning вибирає дію
    StateBeforeMacroAction = HighLevelQL->GetCurrentState();
    CurrentMacroAction = HighLevelQL->ChooseMacroAction(StateBeforeMacroAction);
    
    UE_LOG(LogTemp, Log, TEXT("%s chose macro-action: %d"), 
           *GetName(), (int32)CurrentMacroAction);
    
    ExecuteMacroAction(CurrentMacroAction);
}

void ANPCCharacter::ExecuteMacroAction(EMacroAction MacroAction)
{
    bExecutingMacroAction = true;
    MacroActionStartTime = GetWorld()->GetTimeSeconds();
    
    ENeedType TargetNeed = (ENeedType)((int32)MacroAction);
    
    EActionType ActionType = EActionType::Idle;
    switch (TargetNeed)
    {
        case ENeedType::Hunger:   ActionType = EActionType::UseRefrigerator; break;
        case ENeedType::Bladder:  ActionType = EActionType::UseToilet; break;
        case ENeedType::Energy:   ActionType = EActionType::UseBed; break;
        case ENeedType::Social:   ActionType = EActionType::UseSofa; break;
        case ENeedType::Hygiene:  ActionType = EActionType::UseShower; break;
        case ENeedType::Fun:      ActionType = EActionType::UseTelevision; break;
    }
    
    TArray<AActor*> AvailableObjectsForAction = GetInteractableObjectsForAction(ActionType);
    
    if (AvailableObjectsForAction.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: No objects for action %d"), 
               *GetName(), (int32)ActionType);
        OnMacroActionCompleted(false);
        return;
    }
    
    CurrentTarget = Cast<AInteractableObject>(AvailableObjectsForAction[0]);
    
    float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
    UE_LOG(LogTemp, Warning, TEXT("%s: Distance to target: %.2f"), *GetName(), Distance);
    
    CurrentTarget->OnInteractionComplete.AddDynamic(this, &ANPCCharacter::OnInteractionComplete);
    
    CurrentState = ENPCState::MovingToObject;
    UE_LOG(LogTemp, Log, TEXT("%s moving to %s"), *GetName(), *CurrentTarget->GetName());
    
    if (AIController)
    {
        FAIMoveRequest MoveRequest;
        MoveRequest.SetGoalActor(CurrentTarget);
        MoveRequest.SetAcceptanceRadius(150.0f);
        
        FNavPathSharedPtr NavPath;
        FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest, &NavPath);
        
        if (RequestResult.Code != EPathFollowingRequestResult::RequestSuccessful)
        {
            UE_LOG(LogTemp, Error, TEXT("%s: MoveTo request FAILED!"), *GetName());
            OnMacroActionCompleted(false);
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
    }
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
            OnMacroActionCompleted(false);
        }
    }
    else
    {
        OnMacroActionCompleted(false);
    }
}

void ANPCCharacter::StartInteractionWithObject()
{
    if (!CurrentTarget)
    {
        OnMacroActionCompleted(false);
        return;
    }
    
    if (!CurrentTarget->CanInteract())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: Object became occupied"), *GetName());
        OnMacroActionCompleted(false);
        return;
    }

    CurrentState = ENPCState::Interacting;
    
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
    
    float Reward = CalculateMacroActionReward(true);
    
    FHighLevelState CurrentStateHL = HighLevelQL->GetCurrentState();
    HighLevelQL->UpdateQValue(StateBeforeMacroAction, CurrentMacroAction, Reward, CurrentStateHL);
    
    UCSVLogger::LogAction(NPCID, Generation, CurrentTarget->ActionType, 
                         StateBeforeMacroAction.GetStateKey(),
                         Reward, Lifetime, NeedsComponent->Needs);
    
    CurrentState = ENPCState::Idle;
    if (CurrentTarget)
    {
        CurrentTarget->OnInteractionComplete.RemoveDynamic(this, &ANPCCharacter::OnInteractionComplete);
        CurrentTarget = nullptr;
    }
    
    OnMacroActionCompleted(true);
}

void ANPCCharacter::OnMacroActionCompleted(bool bSuccess)
{
    bExecutingMacroAction = false;
    
    if (!bSuccess)
    {
        CurrentState = ENPCState::Idle;
        if (CurrentTarget)
        {
            CurrentTarget->OnInteractionComplete.RemoveDynamic(this, &ANPCCharacter::OnInteractionComplete);
            CurrentTarget = nullptr;
        }
    }
    
    float Duration = GetWorld()->GetTimeSeconds() - MacroActionStartTime;
    
    UE_LOG(LogTemp, Log, TEXT("%s macro-action completed: Success=%d, Duration=%.1fs"), 
           *GetName(), bSuccess, Duration);
}

float ANPCCharacter::CalculateMacroActionReward(bool bSuccess)
{
    if (!bSuccess)
    {
        return -50.0f;
    }
    
    float Reward = 100.0f;
    
    ENeedType TargetNeed = (ENeedType)((int32)CurrentMacroAction);
    float NeedValue = NeedsComponent->GetNeedValue(TargetNeed);
    
    if (NeedValue >= 80.0f)
    {
        Reward += 50.0f;
    }
    
    for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
    {
        ENeedType Need = (ENeedType)i;
        float Value = NeedsComponent->GetNeedValue(Need);
        
        if (Value < 20.0f)
        {
            Reward -= 30.0f;
        }
    }
    
    return Reward;
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
    
    if (bExecutingMacroAction)
    {
        float Reward = -1000.0f;
        FHighLevelState DeadState = HighLevelQL->GetCurrentState();
        HighLevelQL->UpdateQValue(StateBeforeMacroAction, CurrentMacroAction, Reward, DeadState);
    }
    
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
    Stats.Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H:%M:%S"));

    UGenerationLogger::LogGeneration(Stats);
    
    HighLevelQL->SaveQTable("HighLevelQTable.json");
    
    GetWorld()->GetTimerManager().ClearTimer(DecisionTimerHandle);
    
    SetLifeSpan(2.0f);
}

// Старі функції які більше не використовуються (можна видалити)
TArray<EActionType> ANPCCharacter::GetAvailableActions()
{
    TArray<EActionType> Actions;
    Actions.Add(EActionType::Idle);
    return Actions;
}

AInteractableObject* ANPCCharacter::FindObjectForAction(EActionType Action)
{
    return nullptr;
}

void ANPCCharacter::MoveToObject(AInteractableObject* Object)
{
    // Не використовується
}