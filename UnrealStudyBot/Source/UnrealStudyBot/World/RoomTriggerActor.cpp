#include "World/RoomTriggerActor.h"
#include "UnrealStudyBotGameInstance.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"

ARoomTriggerActor::ARoomTriggerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // ── TriggerBox ────────────────────────────────────────
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 150.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    SetRootComponent(TriggerBox);

    // ── 방 이름 레이블 ────────────────────────────────────
    TxtRoomLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TxtRoomLabel"));
    TxtRoomLabel->SetupAttachment(TriggerBox);
    TxtRoomLabel->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
    TxtRoomLabel->SetHorizontalAlignment(EHTA_Center);
    TxtRoomLabel->SetTextRenderColor(FColor::White);
    TxtRoomLabel->SetWorldSize(48.f);
}

void ARoomTriggerActor::BeginPlay()
{
    Super::BeginPlay();

    // 이름 레이블 설정
    TxtRoomLabel->SetText(FText::FromString(RoomLabel));

    // 트리거 바인딩
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ARoomTriggerActor::OnBeginOverlap);
    TriggerBox->OnComponentEndOverlap.AddDynamic  (this, &ARoomTriggerActor::OnEndOverlap);
}

void ARoomTriggerActor::OnBeginOverlap(
    UPrimitiveComponent*, AActor* Other,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    // 플레이어 캐릭터만 처리
    if (!Cast<ACharacter>(Other)) return;

    m_bPlayerInside = true;
    OnRoomEntered.Broadcast(RoomCategory);

    if (bAutoStartInterview)
        StartInterview();
}

void ARoomTriggerActor::OnEndOverlap(
    UPrimitiveComponent*, AActor* Other,
    UPrimitiveComponent*, int32)
{
    if (!Cast<ACharacter>(Other)) return;

    m_bPlayerInside = false;
    OnRoomExited.Broadcast(RoomCategory);
}

void ARoomTriggerActor::StartInterview()
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;
    GI->OpenInterviewMap(RoomCategory);
}
