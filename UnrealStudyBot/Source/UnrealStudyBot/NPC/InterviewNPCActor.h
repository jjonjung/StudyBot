#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NPC/NPCDialogueComponent.h"
#include "InterviewNPCActor.generated.h"

/**
 * AInterviewNPCActor
 * ─────────────────────────────────────────────────────────
 * 면접관 NPC 캐릭터.
 *  - NPCDialogueComponent로 질문 흐름 제어
 *  - Blueprint에서 애니메이션 / 음성 처리
 *  - InterviewWidget과 OnQuestionChanged 이벤트로 연결
 *
 * Blueprint 확장:
 *   BP_InterviewNPC 로 부모 클래스 설정 후
 *   OnQuestionChanged / OnInterviewDone 바인딩
 */
UCLASS()
class UNREALSTUDYBOT_API AInterviewNPCActor : public ACharacter
{
    GENERATED_BODY()

public:
    AInterviewNPCActor();

    // 외부에서 직접 접근 가능 (Blueprint ReadWrite)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interview")
    UNPCDialogueComponent* DialogueComp;

    // ── Blueprint 이벤트 재정의 ───────────────────────────
    // Blueprint에서 NPC 말풍선 / 애니메이션 연결
    UFUNCTION(BlueprintImplementableEvent, Category="Interview")
    void OnNPCSpeaks(const FString& Text);

    UFUNCTION(BlueprintImplementableEvent, Category="Interview")
    void OnInterviewComplete(const FInterviewResult& Result);

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleQuestionChanged(const FFlashCard& Card);

    UFUNCTION()
    void HandleInterviewDone(const FInterviewResult& Result);
};
