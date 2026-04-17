#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Models/StudyBotTypes.h"
#include "CreateLobbyWidget.generated.h"

/**
 * UCreateLobbyWidget — 로비 생성 폼
 * ─────────────────────────────────────────────────────────
 * 이름 / 카테고리 / 최대 인원 입력 후 방을 생성합니다.
 * LobbySubsystem::OnLobbyCreated 수신 시 LobbyRoomMap으로 이동합니다.
 *
 * Blueprint 바인딩:
 *   InputName      — 방 이름 (EditableTextBox)
 *   CbCategory     — 카테고리 선택 (ComboBoxString)
 *   CbMaxMembers   — 최대 인원 2~6 (ComboBoxString)
 *   BtnCreate      — 생성 버튼
 *   BtnBack        — 뒤로 (Collapsed으로)
 *   TxtMessage     — 오류/상태 메시지
 */
UCLASS()
class UNREALSTUDYBOT_API UCreateLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputName;
    UPROPERTY(meta=(BindWidget)) UComboBoxString*  CbCategory;
    UPROPERTY(meta=(BindWidget)) UComboBoxString*  CbMaxMembers;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnCreate;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnBack;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtMessage;

private:
    UFUNCTION() void OnCreateClicked();
    UFUNCTION() void OnBackClicked();

    UFUNCTION() void HandleLobbyCreated(const FLobbyInfo& Info);
    UFUNCTION() void HandleLobbyError  (int32 Code, const FString& Message);

    void SetButtonsEnabled(bool bEnabled);
    void SetMessage       (const FString& Msg, bool bError = false);
};
