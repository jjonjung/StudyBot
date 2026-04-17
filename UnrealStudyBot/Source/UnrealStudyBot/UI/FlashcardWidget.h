#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Models/StudyBotTypes.h"
#include "FlashcardWidget.generated.h"

/**
 * UFlashcardWidget — 플립 카드 위젯
 * ─────────────────────────────────────────────────────────
 * 앞면(질문) / 뒷면(답변) 구조로 카드를 표시합니다.
 *
 * Blueprint 바인딩:
 *   CardSwitcher      — WidgetSwitcher (0=질문면, 1=답변면)
 *   TxtQuestion       — 질문 텍스트
 *   TxtAnswer         — 답변 텍스트
 *   TxtCategory       — 카테고리 뱃지
 *   TxtProgress       — "3 / 10"
 *   BtnFlip           — 카드 뒤집기
 *   BtnKnown          — 알았어요 (뒷면에 표시)
 *   BtnUnknown        — 몰랐어요 (뒷면에 표시)
 */
UCLASS()
class UNREALSTUDYBOT_API UFlashcardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // 외부에서 카드 데이터 세팅
    UFUNCTION(BlueprintCallable, Category="Flashcard")
    void SetCard(const FFlashCard& Card, int32 CurrentIndex, int32 Total);

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* CardSwitcher;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtQuestion;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtAnswer;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtCategory;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtProgress;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnFlip;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnKnown;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnUnknown;

    // ── 이벤트 (InterviewWidget이 수신) ──────────────────
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardJudged, bool, bKnown);
    UPROPERTY(BlueprintAssignable) FOnCardJudged OnCardJudged;

private:
    UFUNCTION() void OnFlipClicked();
    UFUNCTION() void OnKnownClicked();
    UFUNCTION() void OnUnknownClicked();

    FFlashCard m_currentCard;
};
