#include "UI/InterviewWidget.h"
#include "NPC/NPCDialogueComponent.h"
#include "Subsystem/CardSubsystem.h"
#include "UnrealStudyBotGameInstance.h"

void UInterviewWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnRetry->OnClicked.AddDynamic(this, &UInterviewWidget::OnRetryClicked);
    BtnLobby->OnClicked.AddDynamic(this, &UInterviewWidget::OnLobbyClicked);

    CardWidget->OnCardJudged.AddDynamic(this, &UInterviewWidget::OnCardJudged);

    // NPCDialogueComponent를 직접 소유 (Actor 없이도 동작)
    m_dialogue = NewObject<UNPCDialogueComponent>(this);
    m_dialogue->OnQuestionChanged.AddDynamic(this, &UInterviewWidget::OnQuestionChanged);
    m_dialogue->OnInterviewDone.AddDynamic  (this, &UInterviewWidget::OnInterviewDone);
}

void UInterviewWidget::StartInterview(const TArray<FFlashCard>& Cards)
{
    ScreenSwitcher->SetActiveWidgetIndex(0);
    TxtNPCSpeech->SetText(FText::FromString(
        TEXT("안녕하세요! 오늘 면접을 담당하게 된 NPC입니다.\n준비되시면 카드를 확인해 보세요.")));
    TxtNPCComment->SetText(FText::GetEmpty());

    m_dialogue->StartInterview(Cards);
}

void UInterviewWidget::OnQuestionChanged(const FFlashCard& Card)
{
    int32 Cur   = m_dialogue->GetCurrentIndex();
    int32 Total = m_dialogue->GetTotalCount();

    TxtNPCSpeech->SetText(FText::FromString(
        FString::Printf(TEXT("Q%d. %s"), Cur + 1, *Card.Question)));

    CardWidget->SetCard(Card, Cur, Total);
    TxtNPCComment->SetText(FText::GetEmpty());
}

void UInterviewWidget::OnCardJudged(bool bKnown)
{
    const FFlashCard& Card = m_dialogue->GetCurrentCard();

    // NPC 리액션 표시
    TxtNPCComment->SetText(FText::FromString(m_dialogue->GetNPCComment(bKnown)));

    // 진도 DB 저장
    if (auto* Cards = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        Cards->SaveProgress(Card.Id, bKnown, bKnown ? 5 : 0);

    // 다음 질문으로
    bKnown ? m_dialogue->MarkKnown() : m_dialogue->MarkUnknown();
}

void UInterviewWidget::OnInterviewDone(const FInterviewResult& Result)
{
    m_lastResult = Result;

    // 결과 패널로 전환
    ScreenSwitcher->SetActiveWidgetIndex(1);

    TxtResultCategory->SetText(FText::FromString(Result.Category));
    TxtResultScore->SetText(FText::FromString(
        FString::Printf(TEXT("%d / %d (%.0f%%)"),
            Result.KnownCount, Result.TotalCards, Result.GetScore())));

    // 세션 결과 DB 저장
    if (auto* Cards = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        Cards->SaveInterviewSession(Result);
}

void UInterviewWidget::OnRetryClicked()
{
    // CardSubsystem에서 카드 재로드 후 재시작
    if (auto* Cards = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        StartInterview(Cards->GetCachedCards());
}

void UInterviewWidget::OnLobbyClicked()
{
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->OpenLobbyMap();
}
