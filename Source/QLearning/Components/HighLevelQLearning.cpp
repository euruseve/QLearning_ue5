#include "Components/HighLevelQLearning.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "JsonUtilities.h"

UHighLevelQLearning::UHighLevelQLearning()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHighLevelQLearning::BeginPlay()
{
    Super::BeginPlay();
    
    NeedsComponent = GetOwner()->FindComponentByClass<UNeedsComponent>();
    LoadQTable("HighLevelQTable.json");
}

FHighLevelState UHighLevelQLearning::GetCurrentState() const
{
    FHighLevelState State;
    
    if (NeedsComponent)
    {
        FNPCState CurrentNPCState = NeedsComponent->GetCurrentState();
        State.NeedLevels = CurrentNPCState.NeedLevels;
    }
    
    return State;
}

EMacroAction UHighLevelQLearning::ChooseMacroAction(const FHighLevelState& State)
{
    float RandomValue = FMath::FRand();
    
    if (RandomValue < Params.ExplorationRate)
    {
        int32 RandomIndex = FMath::RandRange(0, (int32)EMacroAction::MAX - 1);
        return (EMacroAction)RandomIndex;
    }
    else
    {
        return GetBestAction(State);
    }
}

EMacroAction UHighLevelQLearning::GetBestAction(const FHighLevelState& State) const
{
    EMacroAction BestAction = EMacroAction::SatisfyHunger;
    float BestQValue = GetQValue(State, BestAction);
    
    for (int32 i = 0; i < (int32)EMacroAction::MAX; i++)
    {
        EMacroAction Action = (EMacroAction)i;
        float QValue = GetQValue(State, Action);
        
        if (QValue > BestQValue)
        {
            BestQValue = QValue;
            BestAction = Action;
        }
    }
    
    return BestAction;
}

void UHighLevelQLearning::UpdateQValue(const FHighLevelState& OldState, 
                                       EMacroAction Action, 
                                       float Reward, 
                                       const FHighLevelState& NewState)
{
    float CurrentQ = GetQValue(OldState, Action);
    float MaxNextQ = GetMaxQValue(NewState);
    
    float NewQ = CurrentQ + Params.LearningRate * 
                 (Reward + Params.DiscountFactor * MaxNextQ - CurrentQ);
    
    SetQValue(OldState, Action, NewQ);
    
    if (Params.ExplorationRate > Params.MinExplorationRate)
    {
        Params.ExplorationRate *= Params.ExplorationDecay;
        Params.ExplorationRate = FMath::Max(Params.ExplorationRate, Params.MinExplorationRate);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("📊 HL Q-Update: State=%s, Action=%d, Reward=%.1f, OldQ=%.1f, NewQ=%.1f, Exploration=%.3f"),
           *OldState.GetStateKey(), (int32)Action, Reward, CurrentQ, NewQ, Params.ExplorationRate);
}

float UHighLevelQLearning::GetQValue(const FHighLevelState& State, EMacroAction Action) const
{
    FString StateKey = State.GetStateKey();
    
    if (QTable.Contains(StateKey))
    {
        const FHighLevelActionValues& ActionValues = QTable[StateKey];
        if (ActionValues.ActionValues.Contains(Action))
        {
            return ActionValues.ActionValues[Action].Value;
        }
    }
    
    return 0.0f;
}

void UHighLevelQLearning::SetQValue(const FHighLevelState& State, EMacroAction Action, float Value)
{
    FString StateKey = State.GetStateKey();
    
    if (!QTable.Contains(StateKey))
    {
        QTable.Add(StateKey, FHighLevelActionValues());
    }
    
    if (!QTable[StateKey].ActionValues.Contains(Action))
    {
        QTable[StateKey].ActionValues.Add(Action, FHighLevelQValue(Value));
    }
    else
    {
        QTable[StateKey].ActionValues[Action].Value = Value;
        QTable[StateKey].ActionValues[Action].TimesVisited++;
    }
}

float UHighLevelQLearning::GetMaxQValue(const FHighLevelState& State) const
{
    float MaxQ = -999999.0f;
    
    for (int32 i = 0; i < (int32)EMacroAction::MAX; i++)
    {
        EMacroAction Action = (EMacroAction)i;
        float QValue = GetQValue(State, Action);
        if (QValue > MaxQ)
        {
            MaxQ = QValue;
        }
    }
    
    return MaxQ == -999999.0f ? 0.0f : MaxQ;
}

void UHighLevelQLearning::SaveQTable(const FString& Filename)
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
    
    UE_LOG(LogTemp, Warning, TEXT("✅ High-Level Q-Table saved: %d states, Path: %s"), 
           QTable.Num(), *FullPath);
}

void UHighLevelQLearning::LoadQTable(const FString& Filename)
{
    FString LoadDirectory = FPaths::ProjectSavedDir() + TEXT("QLearning/");
    FString FullPath = LoadDirectory + Filename;

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not load High-Level Q-Table from: %s"), *FullPath);
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse High-Level Q-Table JSON"));
        return;
    }

    QTable.Empty();

    for (const auto& StatePair : RootObject->Values)
    {
        FString StateKey = StatePair.Key;
        FHighLevelActionValues ActionValues;
        
        const TSharedPtr<FJsonObject>* StateObject;
        if (StatePair.Value->TryGetObject(StateObject))
        {
            for (const auto& ActionPair : (*StateObject)->Values)
            {
                EMacroAction Action = (EMacroAction)FCString::Atoi(*ActionPair.Key);
                
                const TSharedPtr<FJsonObject>* QValueObject;
                if (ActionPair.Value->TryGetObject(QValueObject))
                {
                    FHighLevelQValue QValue;
                    QValue.Value = (*QValueObject)->GetNumberField("Value");
                    QValue.TimesVisited = (*QValueObject)->GetIntegerField("TimesVisited");
                    
                    ActionValues.ActionValues.Add(Action, QValue);
                }
            }
        }
        
        QTable.Add(StateKey, ActionValues);
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ High-Level Q-Table loaded: %d states"), QTable.Num());
}