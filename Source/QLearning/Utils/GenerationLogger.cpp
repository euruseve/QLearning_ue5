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
        CurrentLogFilePath = FPaths::ProjectSavedDir() + TEXT("Logs/Generations_All.csv");
    }
    
    FString Directory = FPaths::GetPath(CurrentLogFilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectory(*Directory);
    }
    
    if (!FPaths::FileExists(CurrentLogFilePath))
    {
        FString Header = TEXT("Timestamp;Generation;NPCID;Lifetime;CauseOfDeath;TotalActions;AvgNeedLevel\n");
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
    
    // ВИПРАВЛЕНО: Форматуємо числа з крапкою
    FString LifetimeStr = FString::SanitizeFloat(Stats.Lifetime).Replace(TEXT(","), TEXT("."));
    FString AvgNeedStr = FString::SanitizeFloat(Stats.AverageNeedLevel).Replace(TEXT(","), TEXT("."));
    
    FString Line = FString::Printf(TEXT("%s;%d;%d;%s;%d;%d;%s\n"),
        *Stats.Timestamp,
        Stats.GenerationNumber,
        Stats.NPCID,
        *LifetimeStr,
        (int32)Stats.CauseOfDeath,
        Stats.TotalActions,
        *AvgNeedStr
    );

    FFileHelper::SaveStringToFile(Line, *CurrentLogFilePath, 
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
    return CurrentLogFilePath;
}

int32 UGenerationLogger::GetLastGeneration()
{
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("Logs/Generations_All.csv");
    
    if (!FPaths::FileExists(FilePath))
    {
        return 0;  
    }
    
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        return 0;
    }
    
    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines);

    if (Lines.Num() <= 1) 
    {
        return 0;
    }
    
    FString LastLine = Lines.Last();
    
    TArray<FString> Columns;
    LastLine.ParseIntoArray(Columns, TEXT(";"));

    if (Columns.Num() >= 2) 
    {
        int32 LastGen = FCString::Atoi(*Columns[1]);
        UE_LOG(LogTemp, Warning, TEXT("Found last generation: %d"), LastGen);
        return LastGen;
    }

    return 0;
}