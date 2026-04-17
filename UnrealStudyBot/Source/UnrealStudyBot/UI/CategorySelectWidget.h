#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Models/StudyBotTypes.h"
#include "CategorySelectWidget.generated.h"

/**
 * UCategorySelectWidget — 로비 화면 (카테고리 선택)
 * ─────────────────────────────────────────────────────────
 * Blueprint 바인딩:
 *   BtnUnreal, BtnCpp, BtnCS, BtnAll, BtnCompany, BtnAlgorithm — 카테고리 버튼
 *   BtnLogout
 *   TxtNickname     — "반갑습니다, {닉네임}님"
 *   TxtLoginType    — "Google 계정" / "로컬 계정" 레이블
 *
 * NOTE: WorldMap 사용 시 이 위젯은 WorldMap HUD에 카테고리 필터로만 남고,
 *       방 선택 자체는 ARoomTriggerActor 오버랩으로 처리됩니다.
 */
UCLASS()
class UNREALSTUDYBOT_API UCategorySelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget)) UButton*    BtnUnreal;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnCpp;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnCS;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnAll;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnCompany;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnAlgorithm;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnLogout;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtNickname;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtLoginType;

private:
    UFUNCTION() void OnUnrealClicked();
    UFUNCTION() void OnCppClicked();
    UFUNCTION() void OnCSClicked();
    UFUNCTION() void OnAllClicked();
    UFUNCTION() void OnCompanyClicked();
    UFUNCTION() void OnAlgorithmClicked();
    UFUNCTION() void OnLogoutClicked();

    void StartInterview(ECardCategory Category);
};
