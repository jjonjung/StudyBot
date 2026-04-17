#include "Subsystem/AuthSubsystem.h"
#include "UnrealStudyBotGameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include <jni.h>
#endif

// ── 전역 포인터 (JNI 콜백용) ──────────────────────────────
// Android JNI 콜백은 UObject 시스템 밖에서 실행되므로
// 정적 포인터로 현재 살아있는 AuthSubsystem에 접근합니다.
static UAuthSubsystem* GAuthSubsystem = nullptr;

// ── 수명주기 ─────────────────────────────────────────────

void UAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GAuthSubsystem = this;
}

void UAuthSubsystem::Deinitialize()
{
    GAuthSubsystem = nullptr;
    Super::Deinitialize();
}

// ── 헬퍼 ─────────────────────────────────────────────────

FString UAuthSubsystem::GetBaseUrl() const
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    return GI ? GI->GetBaseUrl() : TEXT("http://10.0.2.2:3000");
}

// ── Login ─────────────────────────────────────────────────

void UAuthSubsystem::Login(const FString& Username, const FString& Password)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("username"), Username);
    Body->SetStringField(TEXT("password"), Password);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + TEXT("/api/auth/login"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::OnLoginResponse);
    Req->ProcessRequest();
}

void UAuthSubsystem::OnLoginResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid())
    {
        OnLoginResult.Broadcast(false, TEXT("서버에 연결할 수 없습니다."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        OnLoginResult.Broadcast(false, TEXT("응답 파싱 오류"));
        return;
    }

    if (Res->GetResponseCode() != 200)
    {
        FString Err = Json->GetStringField(TEXT("error"));
        OnLoginResult.Broadcast(false, Err);
        return;
    }

    FAuthInfo Info;
    Info.Token    = Json->GetStringField(TEXT("token"));
    Info.Nickname = Json->GetStringField(TEXT("nickname"));
    Info.UserId   = (int32)Json->GetNumberField(TEXT("userId"));

    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->SetAuthInfo(Info);

    OnLoginResult.Broadcast(true, TEXT("로그인 성공"));
}

// ── Register ──────────────────────────────────────────────

void UAuthSubsystem::Register(const FString& Username, const FString& Password,
                               const FString& Nickname)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("username"), Username);
    Body->SetStringField(TEXT("password"), Password);
    Body->SetStringField(TEXT("nickname"), Nickname.IsEmpty() ? Username : Nickname);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + TEXT("/api/auth/register"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::OnRegisterResponse);
    Req->ProcessRequest();
}

void UAuthSubsystem::OnRegisterResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid())
    {
        OnRegisterResult.Broadcast(false, TEXT("서버 연결 실패"));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
    FJsonSerializer::Deserialize(Reader, Json);

    if (Res->GetResponseCode() == 201)
        OnRegisterResult.Broadcast(true, TEXT("회원가입 성공! 로그인해 주세요."));
    else
    {
        FString Err = Json.IsValid() ? Json->GetStringField(TEXT("error")) : TEXT("오류 발생");
        OnRegisterResult.Broadcast(false, Err);
    }
}

// ── Google JNI 로그인 ─────────────────────────────────────

void UAuthSubsystem::LoginWithGoogle()
{
#if PLATFORM_ANDROID
    JNIEnv* Env = FAndroidApplication::GetJavaEnv();
    if (!Env)
    {
        OnLoginResult.Broadcast(false, TEXT("JNI 환경 초기화 실패"));
        return;
    }

    // com.studybot.game.GoogleAuthHelper.signIn(Activity) 호출
    jclass HelperClass = FAndroidApplication::FindJavaClass(
        "com/studybot/game/GoogleAuthHelper");
    if (!HelperClass)
    {
        OnLoginResult.Broadcast(false, TEXT("GoogleAuthHelper 클래스를 찾을 수 없습니다."));
        return;
    }

    jmethodID SignInMethod = Env->GetStaticMethodID(
        HelperClass, "signIn", "(Landroid/app/Activity;)V");
    if (!SignInMethod)
    {
        Env->DeleteLocalRef(HelperClass);
        OnLoginResult.Broadcast(false, TEXT("signIn 메서드를 찾을 수 없습니다."));
        return;
    }

    jobject Activity = FAndroidApplication::GetGameActivityThis();
    Env->CallStaticVoidMethod(HelperClass, SignInMethod, Activity);
    Env->DeleteLocalRef(HelperClass);
#else
    // 에디터/PC: Google 로그인 불가
    OnLoginResult.Broadcast(false, TEXT("Google 로그인은 Android 기기에서만 지원됩니다."));
#endif
}

void UAuthSubsystem::SendGoogleIdTokenToServer(const FString& IdToken)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("idToken"), IdToken);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + TEXT("/api/auth/google/mobile"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::OnGoogleMobileResponse);
    Req->ProcessRequest();
}

void UAuthSubsystem::OnGoogleMobileResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid())
    {
        OnLoginResult.Broadcast(false, TEXT("Google 인증 서버 연결 실패"));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        OnLoginResult.Broadcast(false, TEXT("응답 파싱 오류"));
        return;
    }

    if (Res->GetResponseCode() != 200)
    {
        FString Err = Json->GetStringField(TEXT("error"));
        OnLoginResult.Broadcast(false, Err);
        return;
    }

    FAuthInfo Info;
    Info.Token     = Json->GetStringField(TEXT("token"));
    Info.Nickname  = Json->GetStringField(TEXT("nickname"));
    Info.UserId    = (int32)Json->GetNumberField(TEXT("userId"));
    Info.bIsGoogle = true;
    Json->TryGetStringField(TEXT("avatarUrl"), Info.AvatarUrl);

    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->SetAuthInfo(Info);

    OnLoginResult.Broadcast(true, TEXT("Google 로그인 성공"));
}

// ── Google Sign-Out ───────────────────────────────────────

void UAuthSubsystem::LogoutGoogle()
{
#if PLATFORM_ANDROID
    JNIEnv* Env = FAndroidApplication::GetJavaEnv();
    if (!Env) return;

    jclass HelperClass = FAndroidApplication::FindJavaClass(
        "com/studybot/game/GoogleAuthHelper");
    if (!HelperClass) return;

    jmethodID SignOutMethod = Env->GetStaticMethodID(
        HelperClass, "signOut", "(Landroid/app/Activity;)V");
    if (SignOutMethod)
    {
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        Env->CallStaticVoidMethod(HelperClass, SignOutMethod, Activity);
    }
    Env->DeleteLocalRef(HelperClass);
#endif
}

// ── JNI 콜백 (Android 전용) ───────────────────────────────
// Java: GoogleAuthHelper.nativeOnIdToken(idToken)
// Java: GoogleAuthHelper.nativeOnGoogleSignInError(error)

#if PLATFORM_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_studybot_game_GoogleAuthHelper_nativeOnIdToken(
    JNIEnv* Env, jclass, jstring IdTokenJ)
{
    const char* Raw = Env->GetStringUTFChars(IdTokenJ, nullptr);
    FString IdToken = UTF8_TO_TCHAR(Raw);
    Env->ReleaseStringUTFChars(IdTokenJ, Raw);

    // 게임 스레드에서 Subsystem 호출
    AsyncTask(ENamedThreads::GameThread, [IdToken]()
    {
        if (GAuthSubsystem)
            GAuthSubsystem->SendGoogleIdTokenToServer(IdToken);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_studybot_game_GoogleAuthHelper_nativeOnGoogleSignInError(
    JNIEnv* Env, jclass, jstring ErrorJ)
{
    const char* Raw = Env->GetStringUTFChars(ErrorJ, nullptr);
    FString Err = UTF8_TO_TCHAR(Raw);
    Env->ReleaseStringUTFChars(ErrorJ, Raw);

    AsyncTask(ENamedThreads::GameThread, [Err]()
    {
        if (GAuthSubsystem)
            GAuthSubsystem->OnLoginResult.Broadcast(false, Err);
    });
}
#endif
