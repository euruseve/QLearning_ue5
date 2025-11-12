#include "CSVLogger.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

void UCSVLogger::InitializeLog()
{
    FString FilePath = GetLogFilePath();
    
    FString Directory = FPaths::GetPath(FilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectory(*Directory);
    }

    if (!FPaths::FileExists(FilePath))
    {
        FString Header = TEXT("Timestamp,NPCID,Generation,Action,StateKey,Reward,Lifetime,Event,Hunger,Bladder,Energy,Social,Hygiene,Fun\n");
        FFileHelper::SaveStringToFile(Header, *FilePath);
        UE_LOG(LogTemp, Log, TEXT("CSV Log initialized at: %s"), *FilePath);
    }
}

void UCSVLogger::LogAction(int32 NPCID, int32 Generation, EActionType Action,
                          const FString& StateKey, float Reward, float Lifetime,
                          const TMap<ENeedType, float>& Needs)
{
    FString FilePath = GetLogFilePath();
    
    FString Line = FString::Printf(TEXT("%s,%d,%d,%d,%s,%.2f,%.2f,Action,%s\n"),
        *GetTimestamp(),
        NPCID,
        Generation,
        (int32)Action,
        *StateKey,
        Reward,
        Lifetime,
        *NeedMapToString(Needs)
    );

    FFileHelper::SaveStringToFile(Line, *FilePath, 
                                  FFileHelper::EEncodingOptions::AutoDetect, 
                                  &IFileManager::Get(), 
                                  FILEWRITE_Append);
}

void UCSVLogger::LogDeath(int32 NPCID, int32 Generation, float Lifetime,
                         const TMap<ENeedType, float>& Needs)
{
    FString FilePath = GetLogFilePath();
    
    FString Line = FString::Printf(TEXT("%s,%d,%d,%d,%s,%.2f,%.2f,Death,%s\n"),
        *GetTimestamp(),
        NPCID,
        Generation,
        -1,  // No action
        TEXT("N/A"),  // No state
        -1000.0f,  // Death penalty
        Lifetime,
        *NeedMapToString(Needs)
    );

    FFileHelper::SaveStringToFile(Line, *FilePath, 
                                  FFileHelper::EEncodingOptions::AutoDetect, 
                                  &IFileManager::Get(), 
                                  FILEWRITE_Append);
}

void UCSVLogger::SaveLog()
{
    UE_LOG(LogTemp, Log, TEXT("Log saved"));
}

FString UCSVLogger::GetLogFilePath()
{
    return FPaths::ProjectSavedDir() + TEXT("Logs/QLearning_") + 
           FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".csv");
}

FString UCSVLogger::GetTimestamp()
{
    return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
}

FString UCSVLogger::NeedMapToString(const TMap<ENeedType, float>& Needs)
{
    TArray<float> Values;
    Values.SetNum(6);
    
    for (const auto& Pair : Needs)
    {
        int32 Index = (int32)Pair.Key;
        if (Index >= 0 && Index < 6)
        {
            Values[Index] = Pair.Value;
        }
    }

    return FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
        Values[0], Values[1], Values[2], Values[3], Values[4], Values[5]);
}