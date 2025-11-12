#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../Core/NeedType.h"
#include "GenerationLogger.generated.h"

USTRUCT(BlueprintType)
struct FGenerationStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 GenerationNumber = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 NPCID = 0;

	UPROPERTY(BlueprintReadWrite)
	float Lifetime = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	ENeedType CauseOfDeath;

	UPROPERTY(BlueprintReadWrite)
	int32 TotalActions = 0;

	UPROPERTY(BlueprintReadWrite)
	float AverageNeedLevel = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	FString Timestamp;
};

UCLASS()
class QLEARNING_API UGenerationLogger : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stats")
	static void InitializeGenerationLog();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	static void LogGeneration(const FGenerationStats& Stats);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	static void LogSummary();

private:
	static FString GetGenerationLogPath();
	static int32 TotalGenerations;
	static float TotalLifetime;
	static float BestLifetime;
	static int32 BestGeneration;
};