#include "GenerationLogger.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

int32 UGenerationLogger::TotalGenerations = 0;
float UGenerationLogger::TotalLifetime = 0.0f;
float UGenerationLogger::BestLifetime = 0.0f;
int32 UGenerationLogger::BestGeneration = 0;
FString UGenerationLogger::CurrentLogFilePath = TEXT("");

void UGenerationLogger::InitializeGenerationLog()
{
    if (CurrentLogFilePath.IsEmpty())
    {
        CurrentLogFilePath = FPaths::ProjectSavedDir() + TEXT("Logs/Generations_All.csv");  // ЗМІНЕНО
    }
    
    FString Directory = FPaths::GetPath(CurrentLogFilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectory(*Directory);
    }
    
    if (!FPaths::FileExists(CurrentLogFilePath))
    {
        FString Header = TEXT("Timestamp,Generation,NPCID,Lifetime,CauseOfDeath,TotalActions,AvgNeedLevel\n");
        FFileHelper::SaveStringToFile(Header, *CurrentLogFilePath);
        UE_LOG(LogTemp, Log, TEXT("Generation Log initialized at: %s"), *CurrentLogFilePath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Generation Log appending to existing file: %s"), *CurrentLogFilePath);
    }


    /* FOR STAT RESET
    TotalGenerations = 0;
    TotalLifetime = 0.0f;
    BestLifetime = 0.0f;
    BestGeneration = 0;
    */
}

void UGenerationLogger::LogGeneration(const FGenerationStats& Stats)
{
    if (CurrentLogFilePath.IsEmpty())
    {
        InitializeGenerationLog();
    }
    
    FString Line = FString::Printf(TEXT("%s,%d,%d,%.2f,%d,%d,%.2f\n"),
        *Stats.Timestamp,
        Stats.GenerationNumber,
        Stats.NPCID,
        Stats.Lifetime,
        (int32)Stats.CauseOfDeath,
        Stats.TotalActions,
        Stats.AverageNeedLevel
    );

    FFileHelper::SaveStringToFile(Line, *CurrentLogFilePath,  // ЗМІНЕНО
                                  FFileHelper::EEncodingOptions::AutoDetect, 
                                  &IFileManager::Get(), 
                                  FILEWRITE_Append);
    
    TotalGenerations++;
    TotalLifetime += Stats.Lifetime;

    if (Stats.Lifetime > BestLifetime)
    {
        BestLifetime = Stats.Lifetime;
        BestGeneration = Stats.GenerationNumber;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== GENERATION %d STATS ==="), Stats.GenerationNumber);
    UE_LOG(LogTemp, Warning, TEXT("Lifetime: %.2f seconds"), Stats.Lifetime);
    UE_LOG(LogTemp, Warning, TEXT("Cause of Death: Need %d"), (int32)Stats.CauseOfDeath);
    UE_LOG(LogTemp, Warning, TEXT("Average Lifetime: %.2f seconds"), TotalLifetime / TotalGenerations);
    UE_LOG(LogTemp, Warning, TEXT("Best: Gen %d with %.2f seconds"), BestGeneration, BestLifetime);
}

void UGenerationLogger::LogSummary()
{
    UE_LOG(LogTemp, Warning, TEXT("====== SIMULATION SUMMARY ======"));
    UE_LOG(LogTemp, Warning, TEXT("Total Generations: %d"), TotalGenerations);
    UE_LOG(LogTemp, Warning, TEXT("Average Lifetime: %.2f seconds"), 
           TotalGenerations > 0 ? TotalLifetime / TotalGenerations : 0.0f);
    UE_LOG(LogTemp, Warning, TEXT("Best Generation: %d (%.2f seconds)"), BestGeneration, BestLifetime);
    UE_LOG(LogTemp, Warning, TEXT("================================"));
}

FString UGenerationLogger::GetGenerationLogPath()
{
    return FPaths::ProjectSavedDir() + TEXT("Logs/Generations_") + 
           FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".csv");
}