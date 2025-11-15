#include "QLearningComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"

UQLearningComponent::UQLearningComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    AccumulatedReward = 0.0f;
    PreviousAction = EActionType::Idle;
}

void UQLearningComponent::BeginPlay()
{
    Super::BeginPlay();
    
    NeedsComponent = GetOwner()->FindComponentByClass<UNeedsComponent>();
    
    if (NeedsComponent)
    {
        CurrentState = NeedsComponent->GetCurrentState();
        PreviousState = CurrentState;
    }

    LoadQTable("QTable.json");
}

void UQLearningComponent::TickComponent(float DeltaTime, ELevelTick TickType, 
                                        FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AccumulatedReward -= 0.1f * DeltaTime;
}

void UQLearningComponent::UpdateCurrentState()
{
    if (NeedsComponent)
    {
        PreviousState = CurrentState;
        CurrentState = NeedsComponent->GetCurrentState();
    }
}

EActionType UQLearningComponent::ChooseAction(const TArray<EActionType>& AvailableActions)
{
    if (AvailableActions.Num() == 0)
    {
        return EActionType::Idle;
    }

    float RandomValue = FMath::FRand();
    
    if (RandomValue < Params.ExplorationRate)
    {
        int32 RandomIndex = FMath::RandRange(0, AvailableActions.Num() - 1);
        return AvailableActions[RandomIndex];
    }
    else
    {
        return GetBestAction(CurrentState, AvailableActions);
    }
}

EActionType UQLearningComponent::GetBestAction(const FNPCState& State, 
                                               const TArray<EActionType>& AvailableActions) const
{
    if (AvailableActions.Num() == 0)
    {
        return EActionType::Idle;
    }

    EActionType BestAction = AvailableActions[0];
    float BestQValue = GetQValue(State, BestAction);

    for (const EActionType& Action : AvailableActions)
    {
        float QValue = GetQValue(State, Action);
        if (QValue > BestQValue)
        {
            BestQValue = QValue;
            BestAction = Action;
        }
    }

    return BestAction;
}

void UQLearningComponent::UpdateQValue(float Reward)
{
    if (!NeedsComponent) return;

    // Q(s,a) = Q(s,a) + α * [R + γ * max(Q(s',a')) - Q(s,a)]
    
    float CurrentQ = GetQValue(PreviousState, PreviousAction);
    float MaxNextQ = GetMaxQValue(CurrentState, GetAllActions());
    
    float NewQ = CurrentQ + Params.LearningRate * 
                 (Reward + Params.DiscountFactor * MaxNextQ - CurrentQ);
    
    SetQValue(PreviousState, PreviousAction, NewQ);

    UE_LOG(LogTemp, Log, TEXT("Q-Update: State=%s, Action=%d, Reward=%.2f, OldQ=%.2f, NewQ=%.2f"),
           *PreviousState.GetStateKey(), (int32)PreviousAction, Reward, CurrentQ, NewQ);
}

float UQLearningComponent::CalculateReward(const FNPCState& OldState, 
                                           const FNPCState& NewState, 
                                           bool bDied)
{
    if (bDied)
    {
        return -1000.0f;
    }

    float Reward = 0.0f;

    if (NeedsComponent)
    {
        for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
        {
            ENeedType NeedType = (ENeedType)i;
            
            ENeedLevel OldLevel = OldState.NeedLevels.Contains(NeedType) ? 
                                  OldState.NeedLevels[NeedType] : ENeedLevel::Medium;
            float OldValue = ((int32)OldLevel + 1) * 20.0f;
            
            float NewValue = NeedsComponent->GetNeedValue(NeedType);
            
            float Improvement = NewValue - OldValue;
            Reward += Improvement * 2.0f;  
            
            if (NewValue >= 100.0f && OldValue < 100.0f)
            {
                Reward += 20.0f;  
            }
            
            if (NewValue <= 20.0f)
            {
                Reward -= 50.0f;
            }
            else if (NewValue <= 40.0f)
            {
                Reward -= 20.0f; 
            }
        }
    }
    
    Reward += AccumulatedReward * 0.05f;  
    AccumulatedReward = 0.0f;

    return Reward;
}

float UQLearningComponent::GetQValue(const FNPCState& State, EActionType Action) const
{
    FString StateKey = State.GetStateKey();
    
    if (QTable.Contains(StateKey))
    {
        const FActionQValues& ActionQValues = QTable[StateKey];
        if (ActionQValues.ActionValues.Contains(Action))  
        {
            return ActionQValues.ActionValues[Action].Value; 
        }
    }
    
    return 0.0f;
}

void UQLearningComponent::SetQValue(const FNPCState& State, EActionType Action, float Value)
{
    FString StateKey = State.GetStateKey();
    
    if (!QTable.Contains(StateKey))
    {
        QTable.Add(StateKey, FActionQValues());  
    }
    
    if (!QTable[StateKey].ActionValues.Contains(Action))  
    {
        QTable[StateKey].ActionValues.Add(Action, FQValue(Value));  
    }
    else
    {
        QTable[StateKey].ActionValues[Action].Value = Value;  
        QTable[StateKey].ActionValues[Action].TimesVisited++;  
    }
}

float UQLearningComponent::GetMaxQValue(const FNPCState& State, 
                                        const TArray<EActionType>& AvailableActions) const
{
    float MaxQ = -999999.0f;
    
    for (const EActionType& Action : AvailableActions)
    {
        float QValue = GetQValue(State, Action);
        if (QValue > MaxQ)
        {
            MaxQ = QValue;
        }
    }
    
    return MaxQ == -999999.0f ? 0.0f : MaxQ;
}

void UQLearningComponent::InitializeQValue(const FNPCState& State, EActionType Action)
{
    if (GetQValue(State, Action) == 0.0f)
    {
        SetQValue(State, Action, 0.0f);
    }
}

TArray<EActionType> GetAllActions()
{
    TArray<EActionType> Actions;
    for (int32 i = 0; i < (int32)EActionType::MAX; i++)
    {
        Actions.Add((EActionType)i);
    }
    return Actions;
}

void UQLearningComponent::SaveQTable(const FString& Filename)
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("QLearning/");
    FString FullPath = SaveDirectory + Filename;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectory(*SaveDirectory);
    }

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    for (const auto& StatePair : QTable)
    {
        TSharedPtr<FJsonObject> StateObject = MakeShareable(new FJsonObject);
        
        for (const auto& ActionPair : StatePair.Value.ActionValues)  
        {
            TSharedPtr<FJsonObject> QValueObject = MakeShareable(new FJsonObject);
            QValueObject->SetNumberField("Value", ActionPair.Value.Value);
            QValueObject->SetNumberField("TimesVisited", ActionPair.Value.TimesVisited);
            
            StateObject->SetObjectField(FString::FromInt((int32)ActionPair.Key), QValueObject);
        }
        
        RootObject->SetObjectField(StatePair.Key, StateObject);
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    FFileHelper::SaveStringToFile(OutputString, *FullPath);
    
    UE_LOG(LogTemp, Log, TEXT("Q-Table saved to: %s"), *FullPath);
}

void UQLearningComponent::LoadQTable(const FString& Filename)
{
    FString LoadDirectory = FPaths::ProjectSavedDir() + TEXT("QLearning/");
    FString FullPath = LoadDirectory + Filename;

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not load Q-Table from: %s"), *FullPath);
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse Q-Table JSON"));
        return;
    }

    QTable.Empty();

    for (const auto& StatePair : RootObject->Values)
    {
        FString StateKey = StatePair.Key;
        FActionQValues ActionQValues;  
        
        const TSharedPtr<FJsonObject>* StateObject;
        if (StatePair.Value->TryGetObject(StateObject))
        {
            for (const auto& ActionPair : (*StateObject)->Values)
            {
                EActionType Action = (EActionType)FCString::Atoi(*ActionPair.Key);
                
                const TSharedPtr<FJsonObject>* QValueObject;
                if (ActionPair.Value->TryGetObject(QValueObject))
                {
                    FQValue QValue;
                    QValue.Value = (*QValueObject)->GetNumberField("Value");
                    QValue.TimesVisited = (*QValueObject)->GetIntegerField("TimesVisited");
                    
                    ActionQValues.ActionValues.Add(Action, QValue); 
                }
            }
        }
        
        QTable.Add(StateKey, ActionQValues);  
    }

    UE_LOG(LogTemp, Log, TEXT("Q-Table loaded from: %s (States: %d)"), *FullPath, QTable.Num());
}

TArray<EActionType> UQLearningComponent::GetAllActions() const
{
    TArray<EActionType> Actions;
    for (int32 i = 0; i < (int32)EActionType::MAX; i++)
    {
        Actions.Add((EActionType)i);
    }
    return Actions;
}