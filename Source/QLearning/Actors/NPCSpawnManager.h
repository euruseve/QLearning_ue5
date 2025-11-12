#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Characters/NPCCharacter.h"
#include "NPCSpawnManager.generated.h"



UCLASS()
class QLEARNING_API ANPCSpawnManager : public AActor
{
    GENERATED_BODY()

public:
    ANPCSpawnManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
    int32 TargetNPCCount = 3; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
    TSubclassOf<ANPCCharacter> NPCClass;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
    TArray<class ATargetPoint*> SpawnPoints;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
    float RespawnDelay = 2.0f;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
    bool bAutoStart = true;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bShareQTable = true; 
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalGenerations = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    TArray<ANPCCharacter*> ActiveNPCs;
    
    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void SpawnNPC(int32 NPCID);

    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void StartSimulation();

    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void StopSimulation();

    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void ResetSimulation();

protected:
    UFUNCTION()
    void OnNPCDied(ANPCCharacter* DeadNPC);

    void CheckAndRespawnNPCs();
    FVector GetSpawnLocation(int32 NPCID);

protected:
    UFUNCTION()
    void HandleNPCDeath();  

private:
    ANPCCharacter* CurrentDyingNPC = nullptr; 
    
    bool bIsRunning = false;
    TMap<int32, int32> NPCGenerations; 
    FTimerHandle RespawnCheckTimer;
};