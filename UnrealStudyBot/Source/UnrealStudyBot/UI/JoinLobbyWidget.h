#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Models/StudyBotTypes.h"
#include "JoinLobbyWidget.generated.h"

/**
 * UJoinLobbyWidget — 코드로 로비 입장
 * ─────────────────────────────────────────────────────────
 * 6자리 입장 코드를 입력해 기존 로비에 참가합니다.
 * LobbySubsystem::OnLobbyJoined 수신 시 LobbyRoomMap으로 이동합니다.
 *
 * Blueprint 바인딩:
 *   InputCode    — 6자리 코드 입력 (EditableTextBox)
 *   BtnJoin      — 입장 버튼
 *   BtnBack      — 뒤로 (Collapsed으로)
 *   TxtMessage   — 오류/상태 메시지
 */
UCLASS()
class UNREALSTUDYBOT_API UJoinLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputCode;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnJoin;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnBack;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtMessage;

private:
    UFUNCTION() void OnJoinClicked();
    UFUNCTION() void OnBackClicked();

    UFUNCTION() void HandleLobbyJoined(const FLobbyInfo& Info);
    UFUNCTION() void HandleLobbyError (int32 Code, const FString& Message);

    void SetButtonsEnabled(bool bEnabled);
    void SetMessage       (const FString& Msg, bool bError = false);
};
