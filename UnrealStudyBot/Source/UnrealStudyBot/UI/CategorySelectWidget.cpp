#include "UI/CategorySelectWidget.h"
#include "UnrealStudyBotGameInstance.h"

void UCategorySelectWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnUnreal->OnClicked.AddDynamic    (this, &UCategorySelectWidget::OnUnrealClicked);
    BtnCpp->OnClicked.AddDynamic       (this, &UCategorySelectWidget::OnCppClicked);
    BtnCS->OnClicked.AddDynamic        (this, &UCategorySelectWidget::OnCSClicked);
    BtnAll->OnClicked.AddDynamic       (this, &UCategorySelectWidget::OnAllClicked);
    BtnCompany->OnClicked.AddDynamic   (this, &UCategorySelectWidget::OnCompanyClicked);
    BtnAlgorithm->OnClicked.AddDynamic (this, &UCategorySelectWidget::OnAlgorithmClicked);
    BtnLogout->OnClicked.AddDynamic    (this, &UCategorySelectWidget::OnLogoutClicked);

    // 닉네임 / 로그인 유형 표시
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
    {
        const FAuthInfo& Auth = GI->GetAuthInfo();
        TxtNickname->SetText(FText::FromString(
            FString::Printf(TEXT("반갑습니다, %s님!"), *Auth.Nickname)));
        if (TxtLoginType)
            TxtLoginType->SetText(FText::FromString(
                Auth.bIsGoogle ? TEXT("Google 계정") : TEXT("로컬 계정")));
    }
}

void UCategorySelectWidget::OnUnrealClicked()    { StartInterview(ECardCategory::Unreal);    }
void UCategorySelectWidget::OnCppClicked()       { StartInterview(ECardCategory::Cpp);       }
void UCategorySelectWidget::OnCSClicked()        { StartInterview(ECardCategory::CS);        }
void UCategorySelectWidget::OnAllClicked()       { StartInterview(ECardCategory::All);       }
void UCategorySelectWidget::OnCompanyClicked()   { StartInterview(ECardCategory::Company);   }
void UCategorySelectWidget::OnAlgorithmClicked() { StartInterview(ECardCategory::Algorithm); }

void UCategorySelectWidget::StartInterview(ECardCategory Category)
{
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->OpenInterviewMap(Category);
}

void UCategorySelectWidget::OnLogoutClicked()
{
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->Logout();
}
