#include "UI/LoginWidget.h"
#include "Subsystem/AuthSubsystem.h"
#include "UnrealStudyBotGameInstance.h"
#include "Kismet/GameplayStatics.h"

void ULoginWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnLogin->OnClicked.AddDynamic   (this, &ULoginWidget::OnLoginClicked);
    BtnRegister->OnClicked.AddDynamic(this, &ULoginWidget::OnRegisterClicked);
    if (BtnGoogle)
        BtnGoogle->OnClicked.AddDynamic(this, &ULoginWidget::OnGoogleClicked);

    // AuthSubsystem 이벤트 바인딩
    if (auto* Auth = GetGameInstance()->GetSubsystem<UAuthSubsystem>())
    {
        Auth->OnLoginResult.AddDynamic   (this, &ULoginWidget::HandleLoginResult);
        Auth->OnRegisterResult.AddDynamic(this, &ULoginWidget::HandleRegisterResult);
    }

    if (InputNickname) InputNickname->SetVisibility(ESlateVisibility::Collapsed);
}

void ULoginWidget::OnLoginClicked()
{
    FString User = InputUsername->GetText().ToString().TrimStartAndEnd();
    FString Pass = InputPassword->GetText().ToString();

    if (User.IsEmpty() || Pass.IsEmpty())
    {
        SetMessage(TEXT("아이디와 비밀번호를 입력해 주세요."), true);
        return;
    }

    SetMessage(TEXT("로그인 중..."));
    SetButtonsEnabled(false);

    if (auto* Auth = GetGameInstance()->GetSubsystem<UAuthSubsystem>())
        Auth->Login(User, Pass);
}

void ULoginWidget::OnRegisterClicked()
{
    FString User = InputUsername->GetText().ToString().TrimStartAndEnd();
    FString Pass = InputPassword->GetText().ToString();
    FString Nick = InputNickname ? InputNickname->GetText().ToString() : User;

    if (User.IsEmpty() || Pass.IsEmpty())
    {
        SetMessage(TEXT("아이디와 비밀번호를 입력해 주세요."), true);
        return;
    }

    SetMessage(TEXT("회원가입 중..."));
    SetButtonsEnabled(false);

    if (InputNickname) InputNickname->SetVisibility(ESlateVisibility::Visible);

    if (auto* Auth = GetGameInstance()->GetSubsystem<UAuthSubsystem>())
        Auth->Register(User, Pass, Nick);
}

void ULoginWidget::HandleLoginResult(bool bSuccess, const FString& Message)
{
    SetButtonsEnabled(true);
    if (bSuccess)
    {
        SetMessage(TEXT("환영합니다!"));
        RemoveFromParent();
        if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
            GI->OpenPreLobbyMap();
    }
    else
    {
        SetMessage(Message, true);
    }
}

void ULoginWidget::HandleRegisterResult(bool bSuccess, const FString& Message)
{
    SetButtonsEnabled(true);
    SetMessage(Message, !bSuccess);
    if (bSuccess && InputNickname)
        InputNickname->SetVisibility(ESlateVisibility::Collapsed);
}

void ULoginWidget::SetMessage(const FString& Msg, bool bError)
{
    TxtMessage->SetText(FText::FromString(Msg));
    TxtMessage->SetColorAndOpacity(FSlateColor(
        bError ? FLinearColor(1, 0.2f, 0.2f) : FLinearColor(0.2f, 1, 0.2f)
    ));
}

void ULoginWidget::OnGoogleClicked()
{
    SetMessage(TEXT("Google 로그인 중..."));
    SetButtonsEnabled(false);

    if (auto* Auth = GetGameInstance()->GetSubsystem<UAuthSubsystem>())
        Auth->LoginWithGoogle();
}

void ULoginWidget::SetButtonsEnabled(bool bEnabled)
{
    BtnLogin->SetIsEnabled(bEnabled);
    BtnRegister->SetIsEnabled(bEnabled);
    if (BtnGoogle) BtnGoogle->SetIsEnabled(bEnabled);
}
