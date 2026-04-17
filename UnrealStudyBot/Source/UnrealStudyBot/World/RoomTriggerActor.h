#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Models/StudyBotTypes.h"
#include "RoomTriggerActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomEntered, ECardCategory, Category);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomExited,  ECardCategory, Category);

/**
 * ARoomTriggerActor — 카테고리별 면접 방 트리거
 * ─────────────────────────────────────────────────────────
 * WorldMap에 배치. 플레이어가 진입하면 해당 카테고리의 인터뷰를 시작합니다.
 *
 * Blueprint 설정:
 *  1. RoomCategory 지정 (Unreal / C++ / CS / Company / Algorithm)
 *  2. TriggerBox 크기 조정으로 방 입구 영역 설정
 *  3. OnRoomEntered 이벤트에 InterviewMap 전환 로직 바인딩
 *
 * 방 입장 UI:
 *  - 근접 시 TxtRoomLabel(방 이름) 표시
 *  - 충분히 가까우면 "E - 입장" 프롬프트 표시 (BP 구현)
 */
UCLASS()
class UNREALSTUDYBOT_API ARoomTriggerActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomTriggerActor();

    // ── 에디터 설정 ───────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
    ECardCategory RoomCategory = ECardCategory::Unreal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
    FString RoomLabel = TEXT("면접실");

    // 방 색상: 에디터에서 머티리얼 대신 빠른 프리뷰용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
    FLinearColor RoomColor = FLinearColor(0.1f, 0.3f, 0.8f, 1.f);

    // 트리거 안에 들어왔을 때 자동 인터뷰 시작 여부
    // false면 OnRoomEntered 이벤트만 발행 (UI 프롬프트 → 수동 확인)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
    bool bAutoStartInterview = false;

    // ── 이벤트 ───────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category="Room")
    FOnRoomEntered OnRoomEntered;

    UPROPERTY(BlueprintAssignable, Category="Room")
    FOnRoomExited  OnRoomExited;

    // ── 접근자 ───────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Room")
    void StartInterview();   // 수동 트리거 (E키 입력 등)

    UFUNCTION(BlueprintPure, Category="Room")
    bool IsPlayerInside() const { return m_bPlayerInside; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere) UBoxComponent*        TriggerBox;
    UPROPERTY(VisibleAnywhere) UTextRenderComponent* TxtRoomLabel;

    bool m_bPlayerInside = false;

    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* Comp, AActor* Other,
                        UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex, bool bFromSweep,
                        const FHitResult& Hit);

    UFUNCTION()
    void OnEndOverlap(UPrimitiveComponent* Comp, AActor* Other,
                      UPrimitiveComponent* OtherComp,
                      int32 OtherBodyIndex);
};
