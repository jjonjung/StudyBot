#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Models/StudyBotTypes.h"
#include "StudyBotCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * AStudyBotCharacter — 월드 탐색용 3인칭 캐릭터
 * ─────────────────────────────────────────────────────────
 * WorldMap에서 이동하며 카테고리 방에 입장합니다.
 * Enhanced Input 시스템 사용 (UE5.1+).
 *
 * Blueprint(BP_StudyBotCharacter) 설정:
 *  1. 스켈레탈 메시 & 애니메이션 블루프린트 지정
 *  2. IMC_StudyBot InputMappingContext 지정
 *  3. IA_Move, IA_Look 지정
 *  4. SpringArm / Camera 파라미터 조정
 *
 * 방 입장 흐름:
 *  ARoomTriggerActor::OnRoomEntered → SetCurrentRoom(Category)
 *  → 화면에 "E - 입장" 힌트 표시
 *  → E키 입력 → StartInterviewInCurrentRoom()
 */
UCLASS()
class UNREALSTUDYBOT_API AStudyBotCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AStudyBotCharacter();
    virtual void Tick(float DeltaTime) override;

    // ── 방 상호작용 ───────────────────────────────────────
    // ARoomTriggerActor가 호출 (Overlap 이벤트)
    UFUNCTION(BlueprintCallable, Category="Room")
    void SetCurrentRoom(ECardCategory Category, bool bInside);

    // E키 등으로 현재 방 면접 시작
    UFUNCTION(BlueprintCallable, Category="Room")
    void StartInterviewInCurrentRoom();

    UFUNCTION(BlueprintPure, Category="Room")
    bool IsInsideRoom() const { return m_bInsideRoom; }

    UFUNCTION(BlueprintPure, Category="Room")
    ECardCategory GetCurrentRoomCategory() const { return m_currentRoom; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

    // ── 카메라 ───────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    UCameraComponent* FollowCamera;

    // ── Enhanced Input ────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputMappingContext* IMC_StudyBot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* IA_Move;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* IA_Look;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* IA_Interact; // E키 — 방 입장

private:
    void OnMove   (const FInputActionValue& Value);
    void OnLook   (const FInputActionValue& Value);
    void OnInteract();

    bool          m_bInsideRoom = false;
    ECardCategory m_currentRoom = ECardCategory::All;
};
