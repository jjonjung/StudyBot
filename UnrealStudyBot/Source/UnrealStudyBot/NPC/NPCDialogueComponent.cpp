#include "NPC/NPCDialogueComponent.h"
#include "Math/UnrealMathUtility.h"

void UNPCDialogueComponent::StartInterview(const TArray<FFlashCard>& Cards)
{
    m_deck             = Cards;
    m_currentIndex     = -1;
    m_knownCount       = 0;
    m_bAnswerRevealed  = false;
    NextQuestion();
}

void UNPCDialogueComponent::NextQuestion()
{
    m_bAnswerRevealed = false;
    ++m_currentIndex;

    if (m_currentIndex >= m_deck.Num())
    {
        // 모든 카드 소진 → 결과 통보
        FInterviewResult Result;
        Result.Category   = m_deck.Num() > 0 ? m_deck[0].Category : TEXT("Mixed");
        Result.TotalCards = m_deck.Num();
        Result.KnownCount = m_knownCount;
        OnInterviewDone.Broadcast(Result);
        return;
    }

    OnQuestionChanged.Broadcast(m_deck[m_currentIndex]);
}

void UNPCDialogueComponent::MarkKnown()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_deck.Num())
    {
        m_deck[m_currentIndex].bKnown = true;
        ++m_knownCount;
    }
    AdvanceOrFinish();
}

void UNPCDialogueComponent::MarkUnknown()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_deck.Num())
        m_deck[m_currentIndex].bKnown = false;

    AdvanceOrFinish();
}

void UNPCDialogueComponent::AdvanceOrFinish()
{
    NextQuestion();
}

FFlashCard UNPCDialogueComponent::GetCurrentCard() const
{
    if (m_deck.IsValidIndex(m_currentIndex))
        return m_deck[m_currentIndex];
    return FFlashCard{};
}

FString UNPCDialogueComponent::GetNPCComment(bool bKnown) const
{
    const TArray<FString>& Pool = bKnown ? m_correctComments : m_incorrectComments;
    int32 Idx = FMath::RandRange(0, Pool.Num() - 1);
    return Pool[Idx];
}
