#include "Character/StudyBotCharacter.h"
#include "UnrealStudyBotGameInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

AStudyBotCharacter::AStudyBotCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    // ── 스프링 암 + 카메라 ────────────────────────────────
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength         = 400.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bEnableCameraLag        = true;
    SpringArm->CameraLagSpeed          = 8.f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // ── 이동 설정 ─────────────────────────────────────────
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    auto* Move = GetCharacterMovement();
    Move->bOrientRotationToMovement = true;
    Move->RotationRate              = FRotator(0.f, 500.f, 0.f);
    Move->MaxWalkSpeed              = 500.f;
    Move->MinAnalogWalkSpeed        = 20.f;
    Move->BrakingDecelerationWalking= 2000.f;
}

void AStudyBotCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Enhanced Input 매핑 컨텍스트 등록
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(IMC_StudyBot, 0);
        }
    }
}

void AStudyBotCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AStudyBotCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (auto* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Move,     ETriggerEvent::Triggered, this, &AStudyBotCharacter::OnMove);
        EIC->BindAction(IA_Look,     ETriggerEvent::Triggered, this, &AStudyBotCharacter::OnLook);
        EIC->BindAction(IA_Interact, ETriggerEvent::Triggered, this, &AStudyBotCharacter::OnInteract);
    }
}

// ── 이동 ──────────────────────────────────────────────────

void AStudyBotCharacter::OnMove(const FInputActionValue& Value)
{
    FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void AStudyBotCharacter::OnLook(const FInputActionValue& Value)
{
    FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput  (Axis.X);
    AddControllerPitchInput(Axis.Y);
}

// ── 방 상호작용 ───────────────────────────────────────────

void AStudyBotCharacter::SetCurrentRoom(ECardCategory Category, bool bInside)
{
    m_bInsideRoom = bInside;
    m_currentRoom = bInside ? Category : ECardCategory::All;
}

void AStudyBotCharacter::StartInterviewInCurrentRoom()
{
    if (!m_bInsideRoom) return;
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (GI) GI->OpenInterviewMap(m_currentRoom);
}

void AStudyBotCharacter::OnInteract()
{
    StartInterviewInCurrentRoom();
}
