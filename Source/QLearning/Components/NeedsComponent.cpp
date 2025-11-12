#include "NeedsComponent.h"

UNeedsComponent::UNeedsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    for (int32 i = 0; i < (int32)ENeedType::MAX; i++)
    {
        Needs.Add((ENeedType)i, 75.0f);
    }
}

void UNeedsComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeNeeds();
}

void UNeedsComponent::TickComponent(float DeltaTime, ELevelTick TickType, 
                                    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsAlive)
    {
        DegradeNeeds(DeltaTime);
        CheckForDeath();
    }
}

void UNeedsComponent::InitializeNeeds(float MinValue, float MaxValue)
{
    for (auto& NeedPair : Needs)
    {
        float RandomValue = FMath::RandRange(MinValue, MaxValue);
        Needs[NeedPair.Key] = RandomValue;
    }
}

void UNeedsComponent::DegradeNeeds(float DeltaTime)
{
    for (auto& NeedPair : Needs)
    {
        float NewValue = FMath::Max(0.0f, NeedPair.Value - (DegradationRate * DeltaTime));
        
        if (NewValue != NeedPair.Value)
        {
            Needs[NeedPair.Key] = NewValue;
            OnNeedChanged.Broadcast(NeedPair.Key, NewValue);
        }
    }
}

void UNeedsComponent::CheckForDeath()
{
    for (const auto& NeedPair : Needs)
    {
        if (NeedPair.Value <= 0.0f)
        {
            bIsAlive = false;
            OnNPCDied.Broadcast();
            UE_LOG(LogTemp, Warning, TEXT("NPC Died! Need %d reached 0"), (int32)NeedPair.Key);
            break;
        }
    }
}

void UNeedsComponent::ModifyNeed(ENeedType NeedType, float Amount)
{
    if (!Needs.Contains(NeedType)) return;

    float CurrentValue = Needs[NeedType];
    float NewValue = FMath::Clamp(CurrentValue + Amount, 0.0f, 100.0f);
    
    Needs[NeedType] = NewValue;
    OnNeedChanged.Broadcast(NeedType, NewValue);
}

float UNeedsComponent::GetNeedValue(ENeedType NeedType) const
{
    if (Needs.Contains(NeedType))
    {
        return Needs[NeedType];
    }
    return 0.0f;
}

FNPCState UNeedsComponent::GetCurrentState() const
{
    FNPCState State;
    
    for (const auto& NeedPair : Needs)
    {
        ENeedLevel Level = FNPCState::ValueToLevel(NeedPair.Value);
        State.NeedLevels.Add(NeedPair.Key, Level);
    }
    
    return State;
}

bool UNeedsComponent::IsNeedCritical(ENeedType NeedType) const
{
    float Value = GetNeedValue(NeedType);
    return Value <= 20.0f;
}

ENeedType UNeedsComponent::GetMostCriticalNeed() const
{
    ENeedType MostCritical = ENeedType::Hunger;
    float LowestValue = 100.0f;
    
    for (const auto& NeedPair : Needs)
    {
        if (NeedPair.Value < LowestValue)
        {
            LowestValue = NeedPair.Value;
            MostCritical = NeedPair.Key;
        }
    }
    
    return MostCritical;
}