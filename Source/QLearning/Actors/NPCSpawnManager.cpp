#include "NPCSpawnManager.h"
#include "../Utils/CSVLogger.h"
#include "../Utils/GenerationLogger.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"

ANPCSpawnManager::ANPCSpawnManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ANPCSpawnManager::BeginPlay()
{
    Super::BeginPlay();

    UCSVLogger::InitializeLog();
    UGenerationLogger::InitializeGenerationLog();

    if (SpawnPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No SpawnPoints set, searching for TargetPoints with tag 'NPCSpawn'..."));
        
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ATargetPoint::StaticClass(), 
                                                     FName("NPCSpawn"), FoundActors);
        
        for (AActor* Actor : FoundActors)
        {
            if (ATargetPoint* TP = Cast<ATargetPoint>(Actor))
            {
                SpawnPoints.Add(TP);
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Found %d spawn points"), SpawnPoints.Num());
    }

    if (bAutoStart)
    {
        StartSimulation();
    }

    /* IDK if I gonna use it or not
    if (GetWorld() && GetWorld()->GetWorldSettings())
    {
        GetWorld()->GetWorldSettings()->SetTimeDilation(2.0f);
        UE_LOG(LogTemp, Warning, TEXT("=== TIME DILATION SET TO 2.0x ==="));
    }
     */
}

void ANPCSpawnManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsRunning)
    {
        //CheckAndRespawnNPCs();
    }
}

void ANPCSpawnManager::StartSimulation()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Simulation already running!"));
        return;
    }

    
    bIsRunning = true;
    UE_LOG(LogTemp, Warning, TEXT("=== SIMULATION STARTED ==="));
    UE_LOG(LogTemp, Warning, TEXT("Target NPC Count: %d"), TargetNPCCount);

    for (int32 i = 0; i < TargetNPCCount; i++)
    {
        SpawnNPC(i);
    }
}

void ANPCSpawnManager::StopSimulation()
{
    bIsRunning = false;
    UE_LOG(LogTemp, Warning, TEXT("=== SIMULATION STOPPED ==="));
    
    for (ANPCCharacter* NPC : ActiveNPCs)
    {
        if (IsValid(NPC))  
        {
            NPC->Destroy();
        }
    }
    ActiveNPCs.Empty();
    
    UGenerationLogger::LogSummary();
}

void ANPCSpawnManager::ResetSimulation()
{
    StopSimulation();
    
    TotalGenerations = 0;
    NPCGenerations.Empty();
    
    FString QTablePath = FPaths::ProjectSavedDir() + TEXT("QLearning/QTable.json");
    IFileManager::Get().Delete(*QTablePath);
    
    UE_LOG(LogTemp, Warning, TEXT("=== SIMULATION RESET ==="));
    
    if (bAutoStart)
    {
        FTimerHandle ResetTimer;
        GetWorld()->GetTimerManager().SetTimer(ResetTimer, this, 
                                              &ANPCSpawnManager::StartSimulation, 
                                              1.0f, false);
    }
}

void ANPCSpawnManager::SpawnNPC(int32 NPCID)
{
    if (!NPCClass)
    {
        UE_LOG(LogTemp, Error, TEXT("NPCClass not set in SpawnManager!"));
        return;
    }

    FVector SpawnLocation = GetSpawnLocation(NPCID);
    FRotator SpawnRotation = FRotator::ZeroRotator;
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ANPCCharacter* NewNPC = GetWorld()->SpawnActor<ANPCCharacter>(NPCClass, SpawnLocation, SpawnRotation, SpawnParams);

    if (NewNPC)
    {
        NewNPC->NPCID = NPCID;
        
        if (!NPCGenerations.Contains(NPCID))
        {
            int32 LastGeneration = UGenerationLogger::GetLastGeneration();
            NPCGenerations.Add(NPCID, LastGeneration + 1); 
        }
        else
        {
            NPCGenerations[NPCID]++;
        }
        NewNPC->Generation = NPCGenerations[NPCID];

        TotalGenerations++;
        
        if (NewNPC->NeedsComponent)
        {
            CurrentDyingNPC = NewNPC;
            NewNPC->NeedsComponent->OnNPCDied.AddDynamic(this, &ANPCSpawnManager::HandleNPCDeath);
        }

        if (bShareQTable && NewNPC->QLearningComponent)
        {
            NewNPC->QLearningComponent->LoadQTable("QTable.json"); 
        }

        ActiveNPCs.Add(NewNPC);

        UE_LOG(LogTemp, Warning, TEXT("Spawned NPC %d (Generation %d) at %s"), 
               NPCID, NewNPC->Generation, *SpawnLocation.ToString());
    }
}

void ANPCSpawnManager::HandleNPCDeath()
{
    ANPCCharacter* DeadNPC = nullptr;
    
    for (ANPCCharacter* NPC : ActiveNPCs)
    {
        if (IsValid(NPC) && NPC->NeedsComponent && !NPC->NeedsComponent->bIsAlive)
        {
            DeadNPC = NPC;
            break;
        }
    }

    if (DeadNPC)
    {
        OnNPCDied(DeadNPC);
    }
}

void ANPCSpawnManager::OnNPCDied(ANPCCharacter* DeadNPC)
{
    if (!DeadNPC)
    {
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("Manager: NPC %d died (Generation %d)"), 
           DeadNPC->NPCID, DeadNPC->Generation);
    
    ActiveNPCs.Remove(DeadNPC);
    
    int32 NPCID = DeadNPC->NPCID;
    
    FTimerHandle RespawnTimer;
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer, [this, NPCID]()
    {
        if (bIsRunning)
        {
            SpawnNPC(NPCID);
        }
    }, RespawnDelay, false);
}

void ANPCSpawnManager::CheckAndRespawnNPCs()
{
    ActiveNPCs.RemoveAll([](ANPCCharacter* NPC) 
    { 
        return !IsValid(NPC); 
    });
    
    if (ActiveNPCs.Num() < TargetNPCCount)
    {
        TArray<int32> ActiveIDs;
        for (ANPCCharacter* NPC : ActiveNPCs)
        {
            if (IsValid(NPC))  
            {
                ActiveIDs.Add(NPC->NPCID);
            }
        }
        
        for (int32 i = 0; i < TargetNPCCount; i++)
        {
            if (!ActiveIDs.Contains(i))
            {
                SpawnNPC(i);
                break;
            }
        }
    }
}

FVector ANPCSpawnManager::GetSpawnLocation(int32 NPCID)
{
    if (SpawnPoints.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, SpawnPoints.Num() - 1);
        
        if (IsValid(SpawnPoints[RandomIndex]))
        {
            FVector Location = SpawnPoints[RandomIndex]->GetActorLocation();
            UE_LOG(LogTemp, Log, TEXT("Spawn location from TargetPoint %d: %s"), 
                   RandomIndex, *Location.ToString());
            return Location;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("No valid SpawnPoints! Using random location"));
    return FVector(
        FMath::RandRange(-1000.0f, 1000.0f),
        FMath::RandRange(-1000.0f, 1000.0f),
        100.0f
    );
}