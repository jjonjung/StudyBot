#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "Models/StudyBotTypes.h"
#include "AuthSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResult,   bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterResult, bool, bSuccess, const FString&, Message);

/**
 * UAuthSubsystem
 * ─────────────────────────────────────────────────────────
 * 로그인 / 회원가입 / Google JNI 로그인 담당.
 * GameInstanceSubsystem이므로 앱 전체에서 사용 가능합니다.
 *
 * Google 로그인 흐름 (JNI):
 *   LoginWithGoogle()
 *     → Java GoogleAuthHelper.signIn(Activity)
 *     → GoogleSignInAccount.getIdToken()
 *     → nativeOnIdToken(idToken)  [JNI 콜백]
 *     → SendGoogleIdTokenToServer(idToken)
 *     → POST /api/auth/google/mobile
 *     → OnLoginResult.Broadcast(true, ...)
 */
UCLASS()
class UNREALSTUDYBOT_API UAuthSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── 이벤트 ────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnLoginResult    OnLoginResult;
    UPROPERTY(BlueprintAssignable) FOnRegisterResult OnRegisterResult;

    // ── 로컬 인증 ─────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Auth")
    void Login(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category="Auth")
    void Register(const FString& Username, const FString& Password,
                  const FString& Nickname);

    // ── Google JNI 로그인 ─────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Auth")
    void LoginWithGoogle();

    /** JNI 콜백에서 호출 — 게임 스레드에서 실행됨 */
    void SendGoogleIdTokenToServer(const FString& IdToken);

    /** Google 세션을 로컬에서 해제합니다 (서버 토큰은 별도 처리) */
    UFUNCTION(BlueprintCallable, Category="Auth")
    void LogoutGoogle();

private:
    void OnLoginResponse         (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnRegisterResponse      (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnGoogleMobileResponse  (FHttpRequestPtr, FHttpResponsePtr, bool);

    FString GetBaseUrl() const;
};
