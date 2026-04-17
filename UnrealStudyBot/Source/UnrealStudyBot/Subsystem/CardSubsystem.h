#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "Models/StudyBotTypes.h"
#include "CardSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnCardsLoaded,   const TArray<FFlashCard>&,    Cards);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnHeatmapLoaded, const TArray<FHeatmapEntry>&, Entries);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCardError,     bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE          (FOnProgressSaved);

/**
 * CardSubsystem
 * ─────────────────────────────────────────────────────────
 * 카드 조회 / 진도 저장 / 잔디 데이터 조회 담당.
 * GameInstanceSubsystem이므로 맵 전환 후에도 유지됩니다.
 */
UCLASS()
class UNREALSTUDYBOT_API UCardSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── 이벤트 ────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnCardsLoaded   OnCardsLoaded;
    UPROPERTY(BlueprintAssignable) FOnHeatmapLoaded OnHeatmapLoaded;
    UPROPERTY(BlueprintAssignable) FOnCardError     OnError;
    UPROPERTY(BlueprintAssignable) FOnProgressSaved OnProgressSaved;

    // ── 카드 조회 ─────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Cards")
    void FetchCards(ECardCategory Category);

    UFUNCTION(BlueprintCallable, Category="Cards")
    void FetchInterviewCards(ECardCategory Category, int32 Count = 10);

    // ── 캐시 ─────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="Cards")
    TArray<FFlashCard> GetCachedCards() const { return m_cachedCards; }

    // ── 진도 저장 (fire-and-forget) ───────────────────────
    UFUNCTION(BlueprintCallable, Category="Cards")
    void SaveProgress(int32 CardId, bool bKnown, int32 Score);

    UFUNCTION(BlueprintCallable, Category="Cards")
    void SaveInterviewSession(const FInterviewResult& Result);

    // ── 잔디 조회 ─────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Heatmap")
    void FetchHeatmap(int32 Year = 0); // 0이면 현재 연도

private:
    void OnFetchResponse   (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnProgressResponse(FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnHeatmapResponse (FHttpRequestPtr, FHttpResponsePtr, bool);

    TArray<FFlashCard>    ParseCards  (const FString& JsonStr);
    TArray<FHeatmapEntry> ParseHeatmap(const FString& JsonStr);
    FString               MakeAuthHeader()          const;
    FString               CategoryToString(ECardCategory Cat) const;

    TArray<FFlashCard> m_cachedCards;
};
