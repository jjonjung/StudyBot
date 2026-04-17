#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Models/StudyBotTypes.h"
#include "AlgorithmCardWidget.generated.h"

/**
 * UAlgorithmCardWidget — 알고리즘 전용 플래시카드 위젯
 * ─────────────────────────────────────────────────────────
 * 일반 FlashcardWidget과 별도로, 알고리즘 카드의 추가 정보를 표시합니다.
 *
 * Blueprint 바인딩:
 *   TxtQuestion        — 문제 (예: "이진 탐색을 구현하세요")
 *   TxtCoreConditions  — 핵심 조건 (ScrollBox 안)
 *   TxtSelectionReason — 알고리즘 선택 이유
 *   TxtTimeComplexity  — 시간복잡도 배지 (예: "O(log N)")
 *   TxtAnswer          — 핵심 답변 요약
 *   TxtCodeCpp         — C++ 코드 (Monospace)
 *   TxtCodeCSharp      — C# 코드  (Monospace)
 *   CodeSwitcher       — WidgetSwitcher (0=C++, 1=C#)
 *   BtnCpp / BtnCSharp — 코드 탭 전환 버튼
 *   BtnKnown / BtnUnknown — 판정 버튼
 *   FaceSwitcher       — WidgetSwitcher (0=질문면, 1=답변면)
 *   BtnFlip            — 뒤집기
 *
 * 사용:
 *   SetCard(FFlashCard) → 내용 채움
 *   OnCardJudged 이벤트 → InterviewWidget이 다음 카드로 이동
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlgoCardJudged, bool, bKnown);

UCLASS()
class UNREALSTUDYBOT_API UAlgorithmCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category="AlgoCard")
    void SetCard(const FFlashCard& Card);

    UPROPERTY(BlueprintAssignable, Category="AlgoCard")
    FOnAlgoCardJudged OnCardJudged;

protected:
    // ── 질문면 ────────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* FaceSwitcher;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtQuestion;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtTimeComplexity;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnFlip;

    // ── 답변면 ────────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UScrollBox*  ScrCoreConditions;
    UPROPERTY(meta=(BindWidget)) UTextBlock*  TxtCoreConditions;
    UPROPERTY(meta=(BindWidget)) UTextBlock*  TxtSelectionReason;
    UPROPERTY(meta=(BindWidget)) UTextBlock*  TxtAnswer;

    // ── 코드 탭 ──────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* CodeSwitcher;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtCodeCpp;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtCodeCSharp;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnCpp;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnCSharp;

    // ── 판정 ─────────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UButton* BtnKnown;
    UPROPERTY(meta=(BindWidget)) UButton* BtnUnknown;

private:
    FFlashCard m_card;

    UFUNCTION() void OnFlipClicked();
    UFUNCTION() void OnCppClicked()     { CodeSwitcher->SetActiveWidgetIndex(0); }
    UFUNCTION() void OnCSharpClicked()  { CodeSwitcher->SetActiveWidgetIndex(1); }
    UFUNCTION() void OnKnownClicked()   { OnCardJudged.Broadcast(true);  }
    UFUNCTION() void OnUnknownClicked() { OnCardJudged.Broadcast(false); }
};
