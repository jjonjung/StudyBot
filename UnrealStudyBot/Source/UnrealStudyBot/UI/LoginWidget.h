#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "LoginWidget.generated.h"

/**
 * ULoginWidget
 * ─────────────────────────────────────────────────────────
 * 로그인 / 회원가입 화면
 *
 * Blueprint 바인딩 필요:
 *   InputUsername, InputPassword, InputNickname
 *   BtnLogin, BtnRegister
 *   TxtMessage
 */
UCLASS()
class UNREALSTUDYBOT_API ULoginWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputUsername;
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputPassword;
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputNickname;  // 회원가입 전용
    UPROPERTY(meta=(BindWidget)) UButton*          BtnLogin;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnRegister;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtMessage;

    /** Google 로그인 버튼 (Android 전용, 선택적 바인딩) */
    UPROPERTY(meta=(BindWidgetOptional)) UButton* BtnGoogle;

private:
    UFUNCTION() void OnLoginClicked();
    UFUNCTION() void OnRegisterClicked();
    UFUNCTION() void OnGoogleClicked();

    UFUNCTION() void HandleLoginResult   (bool bSuccess, const FString& Message);
    UFUNCTION() void HandleRegisterResult(bool bSuccess, const FString& Message);

    void SetMessage(const FString& Msg, bool bError = false);
    void SetButtonsEnabled(bool bEnabled);
};
