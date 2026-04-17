#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "UI/FlashcardWidget.h"
#include "Models/StudyBotTypes.h"
#include "InterviewWidget.generated.h"

/**
 * UInterviewWidget — NPC 인터뷰 메인 화면
 * ─────────────────────────────────────────────────────────
 * NPC 말풍선 + FlashcardWidget + 결과 패널로 구성됩니다.
 *
 * Blueprint 바인딩:
 *   ScreenSwitcher     — WidgetSwitcher (0=인터뷰, 1=결과)
 *   TxtNPCSpeech       — NPC 말풍선 텍스트
 *   TxtNPCComment      — NPC 정답/오답 리액션
 *   CardWidget         — FlashcardWidget (자식 위젯)
 *   TxtResultScore     — 결과 점수 "8 / 10 (80%)"
 *   TxtResultCategory  — 카테고리
 *   BtnRetry           — 다시 풀기
 *   BtnLobby           — 로비로 돌아가기
 */
UCLASS()
class UNREALSTUDYBOT_API UInterviewWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // CardSubsystem이 카드를 로드한 후 외부에서 호출
    UFUNCTION(BlueprintCallable, Category="Interview")
    void StartInterview(const TArray<FFlashCard>& Cards);

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* ScreenSwitcher;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtNPCSpeech;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtNPCComment;
    UPROPERTY(meta=(BindWidget)) UFlashcardWidget* CardWidget;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtResultScore;
    UPROPERTY(meta=(BindWidget)) UTextBlock*      TxtResultCategory;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnRetry;
    UPROPERTY(meta=(BindWidget)) UButton*         BtnLobby;

private:
    UFUNCTION() void OnQuestionChanged(const FFlashCard& Card);
    UFUNCTION() void OnInterviewDone  (const FInterviewResult& Result);
    UFUNCTION() void OnCardJudged     (bool bKnown);
    UFUNCTION() void OnRetryClicked();
    UFUNCTION() void OnLobbyClicked();

    class UNPCDialogueComponent* m_dialogue = nullptr;
    FInterviewResult              m_lastResult;
};
