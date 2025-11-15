#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../Core/QLearningTypes.h"
#include "CSVLogger.generated.h"

UCLASS()
class QLEARNING_API UCSVLogger : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Logging")
	static void InitializeLog();

	UFUNCTION(BlueprintCallable, Category = "Logging")
	static void LogAction(int32 NPCID, int32 Generation, EActionType Action, 
						 const FString& StateKey, float Reward, float Lifetime,
						 const TMap<ENeedType, float>& Needs);

	UFUNCTION(BlueprintCallable, Category = "Logging")
	static void LogDeath(int32 NPCID, int32 Generation, float Lifetime,
						const TMap<ENeedType, float>& Needs);

	UFUNCTION(BlueprintCallable, Category = "Logging")
	static void SaveLog();

private:
	static FString GetLogFilePath();
	static FString GetTimestamp();
	static FString NeedMapToString(const TMap<ENeedType, float>& Needs);

	static FString CurrentLogFilePath; 
};