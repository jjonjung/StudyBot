#include "NPC/InterviewNPCActor.h"

AInterviewNPCActor::AInterviewNPCActor()
{
    PrimaryActorTick.bCanEverTick = false;

    DialogueComp = CreateDefaultSubobject<UNPCDialogueComponent>(TEXT("DialogueComp"));
}

void AInterviewNPCActor::BeginPlay()
{
    Super::BeginPlay();

    DialogueComp->OnQuestionChanged.AddDynamic(this, &AInterviewNPCActor::HandleQuestionChanged);
    DialogueComp->OnInterviewDone.AddDynamic  (this, &AInterviewNPCActor::HandleInterviewDone);
}

void AInterviewNPCActor::HandleQuestionChanged(const FFlashCard& Card)
{
    // NPC가 질문을 말풍선으로 "말함"
    FString Prompt = FString::Printf(TEXT("Q%d. %s"),
        DialogueComp->GetCurrentIndex() + 1, *Card.Question);
    OnNPCSpeaks(Prompt);
}

void AInterviewNPCActor::HandleInterviewDone(const FInterviewResult& Result)
{
    FString Summary = FString::Printf(
        TEXT("수고하셨습니다! %d문제 중 %d개를 맞추셨습니다. (%.0f%%)"),
        Result.TotalCards, Result.KnownCount, Result.GetScore());
    OnNPCSpeaks(Summary);
    OnInterviewComplete(Result);
}
