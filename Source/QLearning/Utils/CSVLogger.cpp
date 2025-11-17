#include "CSVLogger.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/Culture.h"

FString UCSVLogger::CurrentLogFilePath = TEXT("");

void UCSVLogger::InitializeLog()
{
    if (CurrentLogFilePath.IsEmpty())
    {
        CurrentLogFilePath = FPaths::ProjectSavedDir() + TEXT("Logs/QLearning_All.csv");
    }

    FString Directory = FPaths::GetPath(CurrentLogFilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectory(*Directory);
    }
    
    if (!FPaths::FileExists(CurrentLogFilePath))
    {
        FString Header = TEXT("Timestamp;NPCID;Generation;Action;StateKey;Reward;Lifetime;Event;Hunger;Bladder;Energy;Social;Hygiene;Fun\n");
        FFileHelper::SaveStringToFile(Header, *CurrentLogFilePath);
        UE_LOG(LogTemp, Log, TEXT("CSV Log initialized at: %s"), *CurrentLogFilePath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CSV Log appending to existing file: %s"), *CurrentLogFilePath);
    }
}

void UCSVLogger::LogAction(int32 NPCID, int32 Generation, EActionType Action,
                          const FString& StateKey, float Reward, float Lifetime,
                          const TMap<ENeedType, float>& Needs)
{
    if (CurrentLogFilePath.IsEmpty())
    {
        InitializeLog();  
    }
    
    // ВИПРАВЛЕНО: Форматуємо числа з крапкою
    FString RewardStr = FString::SanitizeFloat(Reward).Replace(TEXT(","), TEXT("."));
    FString LifetimeStr = FString::SanitizeFloat(Lifetime).Replace(TEXT(","), TEXT("."));
    
    FString Line = FString::Printf(TEXT("%s;%d;%d;%d;%s;%s;%s;Action;%s\n"),
        *GetTimestamp(),
        NPCID,
        Generation,
        (int32)Action,
        *StateKey,
        *RewardStr,
        *LifetimeStr,
        *NeedMapToString(Needs)
    );

    FFileHelper::SaveStringToFile(Line, *CurrentLogFilePath, 
                                  FFileHelper::EEncodingOptions::AutoDetect, 
                                  &IFileManager::Get(), 
                                  FILEWRITE_Append);
}

void UCSVLogger::LogDeath(int32 NPCID, int32 Generation, float Lifetime,
                         const TMap<ENeedType, float>& Needs)
{
    if (CurrentLogFilePath.IsEmpty())
    {
        InitializeLog();
    }
    
    // ВИПРАВЛЕНО: Форматуємо числа з крапкою
    FString LifetimeStr = FString::SanitizeFloat(Lifetime).Replace(TEXT(","), TEXT("."));
    
    FString Line = FString::Printf(TEXT("%s;%d;%d;%d;%s;-1000.00;%s;Death;%s\n"),
        *GetTimestamp(),
        NPCID,
        Generation,
        -1,
        TEXT("N/A"),
        *LifetimeStr,
        *NeedMapToString(Needs)
    );

    FFileHelper::SaveStringToFile(Line, *CurrentLogFilePath, 
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
    return CurrentLogFilePath;
}

FString UCSVLogger::GetTimestamp()
{
    return FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H:%M:%S"));
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

    // ВИПРАВЛЕНО: Кожне число окремо з заміною коми на крапку
    FString V0 = FString::SanitizeFloat(Values[0]).Replace(TEXT(","), TEXT("."));
    FString V1 = FString::SanitizeFloat(Values[1]).Replace(TEXT(","), TEXT("."));
    FString V2 = FString::SanitizeFloat(Values[2]).Replace(TEXT(","), TEXT("."));
    FString V3 = FString::SanitizeFloat(Values[3]).Replace(TEXT(","), TEXT("."));
    FString V4 = FString::SanitizeFloat(Values[4]).Replace(TEXT(","), TEXT("."));
    FString V5 = FString::SanitizeFloat(Values[5]).Replace(TEXT(","), TEXT("."));

    return FString::Printf(TEXT("%s;%s;%s;%s;%s;%s"),
        *V0, *V1, *V2, *V3, *V4, *V5);
}