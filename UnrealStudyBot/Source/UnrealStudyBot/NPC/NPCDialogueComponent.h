#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Models/StudyBotTypes.h"
#include "NPCDialogueComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestionChanged, const FFlashCard&, Card);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInterviewDone,   const FInterviewResult&, Result);

/**
 * NPCDialogueComponent
 * ─────────────────────────────────────────────────────────
 * NPC가 면접관 역할을 하며 질문을 순차적으로 제시합니다.
 *
 * 흐름:
 *   StartInterview(카드목록) → OnQuestionChanged(첫 질문)
 *   → NextQuestion / MarkKnown / MarkUnknown
 *   → 마지막 카드 후 OnInterviewDone(결과)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALSTUDYBOT_API UNPCDialogueComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // ── 이벤트 ────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnQuestionChanged OnQuestionChanged;
    UPROPERTY(BlueprintAssignable) FOnInterviewDone   OnInterviewDone;

    // ── 제어 ─────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Interview")
    void StartInterview(const TArray<FFlashCard>& Cards);

    UFUNCTION(BlueprintCallable, Category="Interview")
    void MarkKnown();

    UFUNCTION(BlueprintCallable, Category="Interview")
    void MarkUnknown();

    UFUNCTION(BlueprintCallable, Category="Interview")
    void NextQuestion();

    // ── 상태 조회 ─────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="Interview")
    FFlashCard GetCurrentCard()   const;

    UFUNCTION(BlueprintPure, Category="Interview")
    int32      GetCurrentIndex()  const { return m_currentIndex; }

    UFUNCTION(BlueprintPure, Category="Interview")
    int32      GetTotalCount()    const { return m_deck.Num(); }

    UFUNCTION(BlueprintPure, Category="Interview")
    bool       IsAnswerRevealed() const { return m_bAnswerRevealed; }

    UFUNCTION(BlueprintCallable, Category="Interview")
    void       RevealAnswer()          { m_bAnswerRevealed = true; }

    // NPC 멘트 — 정답/오답/질문 시작 시 표시할 대사
    UFUNCTION(BlueprintPure, Category="Interview")
    FString GetNPCComment(bool bKnown) const;

private:
    void AdvanceOrFinish();

    TArray<FFlashCard> m_deck;
    int32              m_currentIndex   = -1;
    int32              m_knownCount     = 0;
    bool               m_bAnswerRevealed = false;

    // NPC 멘트 풀
    TArray<FString> m_correctComments  = {
        TEXT("잘 알고 계시네요! 다음으로 넘어가겠습니다."),
        TEXT("정확합니다. 실력이 좋으시군요."),
        TEXT("훌륭해요. 핵심을 파악하고 계십니다."),
    };
    TArray<FString> m_incorrectComments = {
        TEXT("아직 조금 더 공부가 필요할 것 같습니다."),
        TEXT("이 부분은 다시 한번 복습해 보세요."),
        TEXT("어렵죠. 모범 답변을 잘 기억해 두세요."),
    };
};
