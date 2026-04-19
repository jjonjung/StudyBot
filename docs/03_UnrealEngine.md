# 03. Unreal Engine 5.6 코드 흐름 및 클래스 상세 (v3)

## 클래스 계층도

```
UGameInstance
  └─ UStudyBotGameInstance      ← 전역 상태 (토큰·로비코드·IsHost·씬전환)

UGameInstanceSubsystem
  ├─ UAuthSubsystem             ← 로컬 로그인/회원가입 + Google JNI 연동
  ├─ UCardSubsystem             ← 카드 조회·진도·히트맵 HTTP
  └─ ULobbySubsystem ★신규      ← 로비 REST + WebSocket 실시간

ACharacter
  ├─ AInterviewNPCActor         ← NPC 캐릭터 (InterviewMap)
  │    └─ UNPCDialogueComponent ← 질문 흐름 제어 (ActorComponent)
  └─ AStudyBotCharacter         ← 플레이어 캐릭터 (WorldMap 탐색)

AActor
  └─ ARoomTriggerActor          ← 방 입장 감지 (BoxComponent 오버랩)

UUserWidget
  ├─ ULoginWidget               ← 로그인 화면 (로컬 + Google 버튼)
  ├─ UPreLobbyWidget ★신규      ← 방 만들기 / 코드 입장 선택
  ├─ UCreateLobbyWidget ★신규   ← 로비 생성 폼
  ├─ UJoinLobbyWidget ★신규     ← 6자리 코드 입력
  ├─ ULobbyRoomWidget ★신규     ← 멤버 목록·채팅·준비·시작
  ├─ UFlashcardWidget           ← 플립 카드 UI
  ├─ UAlgorithmCardWidget       ← Algorithm 전용 카드 (코드·복잡도)
  ├─ UInterviewWidget           ← NPC 인터뷰 메인 화면
  └─ UContributionWidget        ← GitHub 잔디 히트맵 (53×7 그리드)

Models/StudyBotTypes.h          ← 공용 구조체·열거형 (v3 확장)
```

---

## Models/StudyBotTypes.h — 공용 타입 (v3)

```cpp
// ── 카테고리 ──────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ECardCategory : uint8 {
    All, Unreal, Cpp, CS, Company, Algorithm
};

// ── 로비 역할 ────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ELobbyRole : uint8 { Host, Member };

// ── 로비 상태 ────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ELobbyStatus : uint8 { Waiting, InProgress, Closed };

// ── 플래시카드 (Algorithm 전용 필드 포함) ─────────────────────
USTRUCT(BlueprintType)
struct FFlashCard {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32   Id          = 0;
    UPROPERTY(BlueprintReadOnly) FString Category, Company;
    UPROPERTY(BlueprintReadOnly) FString Question, Answer, Difficulty;
    // Algorithm 전용 — 다른 카테고리는 빈 문자열
    UPROPERTY(BlueprintReadOnly) FString CoreConditions;
    UPROPERTY(BlueprintReadOnly) FString SelectionReason;
    UPROPERTY(BlueprintReadOnly) FString CodeCpp;
    UPROPERTY(BlueprintReadOnly) FString CodeCSharp;
    UPROPERTY(BlueprintReadOnly) FString TimeComplexity;
    // 세션 내 임시 (DB 저장 안 함)
    bool  bKnown = false;
    int32 Score  = 0;
};

// ── 로비 멤버 ────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FLobbyMember {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32      UserId   = 0;
    UPROPERTY(BlueprintReadOnly) FString    Nickname;
    UPROPERTY(BlueprintReadOnly) ELobbyRole Role     = ELobbyRole::Member;
    UPROPERTY(BlueprintReadOnly) bool       bIsReady = false;

    bool IsHost() const { return Role == ELobbyRole::Host; }
};

// ── 로비 정보 ────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FLobbyInfo {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32              LobbyId    = 0;
    UPROPERTY(BlueprintReadOnly) FString            Code;        // "ABC123" (6자리)
    UPROPERTY(BlueprintReadOnly) FString            Name;
    UPROPERTY(BlueprintReadOnly) FString            Category;
    UPROPERTY(BlueprintReadOnly) int32              MaxMembers = 4;
    UPROPERTY(BlueprintReadOnly) TArray<FLobbyMember> Members;

    bool IsValid() const { return LobbyId > 0; }
};

// ── 잔디 히트맵 엔트리 ──────────────────────────────────────
USTRUCT(BlueprintType)
struct FHeatmapEntry {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString ScoreDate;   // "2025-07-01"
    UPROPERTY(BlueprintReadOnly) FString Category;
    UPROPERTY(BlueprintReadOnly) int32   CardsDone  = 0;
    UPROPERTY(BlueprintReadOnly) int32   KnownCount = 0;
    UPROPERTY(BlueprintReadOnly) float   Ratio      = 0.f; // 0.0~1.0
};

// ── 인증 정보 ────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FAuthInfo {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Token;
    UPROPERTY(BlueprintReadOnly) FString Nickname;
    UPROPERTY(BlueprintReadOnly) int32   UserId    = 0;
    UPROPERTY(BlueprintReadOnly) FString AvatarUrl; // Google 프로필 이미지
    UPROPERTY(BlueprintReadOnly) bool    bIsGoogle = false;

    bool IsValid() const { return !Token.IsEmpty(); }
};
```

---

## UStudyBotGameInstance — 전역 상태 (v3 확장)

```cpp
class UStudyBotGameInstance : public UGameInstance
{
    FAuthInfo     m_authInfo;
    FString       m_baseUrl;            // HTTP (DefaultGame.ini)
    FString       m_wsBaseUrl;          // WebSocket (DefaultGame.ini)
    ECardCategory m_selectedCategory;
    FLobbyInfo    m_currentLobby;
    bool          m_bIsHost = false;

public:
    // 인증
    void      SetAuthInfo(const FAuthInfo& Info);
    FAuthInfo GetAuthInfo() const { return m_authInfo; }
    bool      IsLoggedIn()  const { return m_authInfo.IsValid(); }
    void      Logout();

    // 로비
    void       SetLobbyInfo(const FLobbyInfo& Info, bool bIsHost);
    FLobbyInfo GetLobbyInfo() const { return m_currentLobby; }
    bool       IsHost()       const { return m_bIsHost; }
    void       ClearLobbyInfo();

    // URL
    FString GetBaseUrl()   const { return m_baseUrl; }
    FString GetWsBaseUrl() const { return m_wsBaseUrl; }

    // 씬 전환
    void OpenLoginMap();
    void OpenPreLobbyMap();      // LoginMap → PreLobbyMap (로컬/Google 로그인 후)
    void OpenLobbyRoomMap();     // PreLobbyMap → LobbyRoomMap (로비 생성·입장 후)
    void OpenInterviewMap(ECardCategory Category);
    void OpenLobbyMap() { OpenPreLobbyMap(); }  // 하위 호환
};
```

**`m_wsBaseUrl` 추가 이유:**
HTTP와 WebSocket 기본 URL이 동일 서버지만 scheme이 다릅니다 (`http://` vs `ws://`).
`DefaultGame.ini`에서 별도로 읽어 코드에서 조합하지 않습니다.

---

## UAuthSubsystem — 로컬 + Google JNI

```cpp
UCLASS()
class UAuthSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // 로컬 로그인 (기존 유지)
    void Register(const FString& Username, const FString& Password, const FString& Nickname);
    void Login(const FString& Username, const FString& Password);

    // v3 신규 — Google JNI
    void LoginWithGoogle();
    void SendGoogleIdTokenToServer(const FString& IdToken);

    // Delegates
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResult, bool, bSuccess, const FString&, Message);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterResult, bool, bSuccess, const FString&, Message);

    FOnLoginResult   OnLoginResult;
    FOnRegisterResult OnRegisterResult;

private:
    void OnLoginResponse(FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnGoogleMobileResponse(FHttpRequestPtr, FHttpResponsePtr, bool);
};
```

### LoginWithGoogle — JNI 브리지 호출

```cpp
// .cpp 파일 상단 — JNI 콜백 수신용 정적 포인터
static UAuthSubsystem* GAuthSubsystem = nullptr;

void UAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GAuthSubsystem = this;  // JNI 콜백에서 접근 가능하게 등록
}

void UAuthSubsystem::Deinitialize()
{
    GAuthSubsystem = nullptr;
    Super::Deinitialize();
}

void UAuthSubsystem::LoginWithGoogle()
{
#if PLATFORM_ANDROID
    JNIEnv* Env = FAndroidApplication::GetJavaEnv();
    jclass  Cls = FAndroidApplication::FindJavaClass(
        "com/studybot/game/GoogleAuthHelper");
    jmethodID Mid = Env->GetStaticMethodID(
        Cls, "signIn", "(Landroid/app/Activity;)V");
    Env->CallStaticVoidMethod(Cls, Mid,
        FAndroidApplication::GetGameActivityThis());
    Env->DeleteLocalRef(Cls);
#else
    OnLoginResult.Broadcast(false,
        TEXT("Google 로그인은 Android 기기에서만 지원됩니다."));
#endif
}

// Java → C++ 콜백 (GAuthSubsystem 정적 포인터 경유)
extern "C" JNIEXPORT void JNICALL
Java_com_studybot_game_GoogleAuthHelper_nativeOnIdToken(
    JNIEnv* Env, jclass, jstring JIdToken)
{
    const char* Raw = Env->GetStringUTFChars(JIdToken, nullptr);
    FString IdToken = UTF8_TO_TCHAR(Raw);
    Env->ReleaseStringUTFChars(JIdToken, Raw);

    AsyncTask(ENamedThreads::GameThread, [IdToken]() {
        if (GAuthSubsystem)
            GAuthSubsystem->SendGoogleIdTokenToServer(IdToken);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_studybot_game_GoogleAuthHelper_nativeOnGoogleSignInError(
    JNIEnv* Env, jclass, jstring JError)
{
    const char* Raw = Env->GetStringUTFChars(JError, nullptr);
    FString Err = UTF8_TO_TCHAR(Raw);
    Env->ReleaseStringUTFChars(JError, Raw);

    AsyncTask(ENamedThreads::GameThread, [Err]() {
        if (GAuthSubsystem)
            GAuthSubsystem->OnLoginResult.Broadcast(false, Err);
    });
}
```

### SendGoogleIdTokenToServer

```cpp
void UAuthSubsystem::SendGoogleIdTokenToServer(const FString& IdToken)
{
    // POST /api/auth/google/mobile { idToken }
    // 응답: { token, nickname, userId }
    // → GameInstance::SetAuthInfo(bIsGoogle=true)
    // → OnLoginResult.Broadcast(true, "Google 로그인 성공")
}
```

---

## ULobbySubsystem ★신규 — 로비 REST + WebSocket

```cpp
UCLASS()
class ULobbySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    TSharedPtr<IWebSocket>    m_ws;
    bool                      m_bWsConnected = false;
    TMap<int32, FLobbyMember> m_memberMap;   // UserId → Member (로컬 캐시)

    // O(1) WebSocket 이벤트 라우터 (Initialize에서 등록)
    using FWsHandler = TFunction<void(const TSharedPtr<FJsonObject>&)>;
    TMap<FString, FWsHandler> m_wsHandlers;

public:
    virtual void Initialize  (FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── REST API ──────────────────────────────────────────────
    void CreateLobby(const FString& Name, const FString& Category, int32 MaxMembers = 4);
    void JoinLobbyByCode(const FString& Code);
    void KickMember(int32 TargetUserId);   // Host 전용
    void LeaveLobby();
    void CloseLobby();                     // Host 전용

    // ── WebSocket 이벤트 전송 ────────────────────────────────
    void SendChatMessage(const FString& Message);
    void SendSetCategory(const FString& Category);   // Host 전용
    void SendStartInterview();                        // Host 전용
    void SendReady(bool bReady);

    // ── Delegates ────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnLobbyCreated       OnLobbyCreated;
    UPROPERTY(BlueprintAssignable) FOnLobbyJoined        OnLobbyJoined;
    UPROPERTY(BlueprintAssignable) FOnLobbyError         OnLobbyError;       // (int32 Code, FString Message)
    UPROPERTY(BlueprintAssignable) FOnMemberJoined       OnMemberJoined;
    UPROPERTY(BlueprintAssignable) FOnMemberLeft         OnMemberLeft;
    UPROPERTY(BlueprintAssignable) FOnMemberKicked       OnMemberKicked;
    UPROPERTY(BlueprintAssignable) FOnChatReceived       OnChatReceived;
    UPROPERTY(BlueprintAssignable) FOnLobbyStarted       OnLobbyStarted;     // (FString Category, int32 CardCount)
    UPROPERTY(BlueprintAssignable) FOnCategoryChanged    OnCategoryChanged;
    UPROPERTY(BlueprintAssignable) FOnMemberReadyChanged OnMemberReadyChanged;

    // ── 유틸 ────────────────────────────────────────────────
    TArray<FLobbyMember> GetSortedMembers() const;  // Host 먼저, 나머지 UserId 오름차순
    bool IsConnected() const { return m_bWsConnected; }

private:
    // WebSocket 연결은 REST 응답 후 내부에서 자동 호출
    void ConnectWebSocket(int32 LobbyId);
    void DisconnectWebSocket();
    void OnWsConnected();
    void OnWsMessage(const FString& Msg);
    void OnWsError  (const FString& Err);
    void OnWsClosed (int32 StatusCode, const FString& Reason, bool bWasClean);
    void InitWsHandlers();   // m_wsHandlers 등록 (Initialize에서 호출)
};
```

### WebSocket 이벤트 디스패처 — 인스턴스 TMap O(1) 분기

```cpp
void ULobbySubsystem::InitWsHandlers()
{
    // Initialize()에서 1회 등록 — static 아닌 인스턴스 TMap
    m_wsHandlers.Add(TEXT("chat/message"),
        [this](const TSharedPtr<FJsonObject>& P) {
            OnChatReceived.Broadcast(
                P->GetStringField(TEXT("nickname")),
                P->GetStringField(TEXT("message")));
        });

    m_wsHandlers.Add(TEXT("member/joined"),
        [this](const TSharedPtr<FJsonObject>& P) {
            FLobbyMember M = ParseMember(P);
            m_memberMap.Add(M.UserId, M);
            OnMemberJoined.Broadcast(M);
        });

    m_wsHandlers.Add(TEXT("member/kicked"),
        [this](const TSharedPtr<FJsonObject>& P) {
            int32 Id = (int32)P->GetNumberField(TEXT("userId"));
            m_memberMap.Remove(Id);
            OnMemberKicked.Broadcast(Id);
            // 자신이 강퇴됐으면 자동으로 PreLobbyMap 이동
            auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
            if (GI && GI->GetAuthInfo().UserId == Id) {
                DisconnectWebSocket();
                GI->ClearLobbyInfo();
                GI->OpenPreLobbyMap();
            }
        });

    m_wsHandlers.Add(TEXT("lobby/started"),
        [this](const TSharedPtr<FJsonObject>& P) {
            // LobbyRoomWidget이 OnLobbyStarted를 받아 카드 로딩 + 맵 이동 처리
            OnLobbyStarted.Broadcast(
                P->GetStringField(TEXT("category")),
                (int32)P->GetNumberField(TEXT("cardCount")));
        });

    m_wsHandlers.Add(TEXT("lobby/category_changed"),
        [this](const TSharedPtr<FJsonObject>& P) {
            OnCategoryChanged.Broadcast(P->GetStringField(TEXT("category")));
        });

    m_wsHandlers.Add(TEXT("lobby/closed"),
        [this](const TSharedPtr<FJsonObject>&) {
            DisconnectWebSocket();
            if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance())) {
                GI->ClearLobbyInfo();
                GI->OpenPreLobbyMap();
            }
        });

    // OnWsMessage에서 라우팅
    // if (const FWsHandler* H = m_wsHandlers.Find(Event)) (*H)(Payload);
}
```

### GetSortedMembers — Host 우선 정렬

```cpp
TArray<FLobbyMember> ULobbySubsystem::GetSortedMembers() const
{
    TArray<FLobbyMember> Out;
    m_memberMap.GenerateValueArray(Out);
    Out.Sort([](const FLobbyMember& A, const FLobbyMember& B) {
        if (A.Role != B.Role) return A.Role == ELobbyRole::Host;
        return A.UserId < B.UserId;  // 입장 순서 근사
    });
    return Out;
}
```

---

## UCardSubsystem — 카드 조회·진도·히트맵 (기존 유지)

```cpp
// 주요 메서드 (변경 없음)
void FetchInterviewCards(const FString& Category, int32 Count = 10);
void SaveProgress(int32 CardId, bool bKnown, int32 Score);
void SaveInterviewSession(const FString& Category, int32 Total, int32 Known);
void FetchHeatmap(int32 Year = 0);

// Algorithm 필드 파싱 (TryGetStringField — 부재 시 빈 문자열)
FFlashCard ParseSingleCard(const TSharedPtr<FJsonObject>& Obj);
```

**로컬 캐시 `m_cachedCards`:**
인터뷰 중 네트워크 단절 시 데이터 유지, 재시도 시 서버 재요청 없이 즉시 재사용.

---

## ARoomTriggerActor — WorldMap 방 입장 감지

```cpp
UCLASS()
class ARoomTriggerActor : public AActor
{
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ECardCategory RoomCategory = ECardCategory::Unreal;
    UPROPERTY(EditAnywhere) bool bAutoStartInterview = false;

    UPROPERTY(BlueprintAssignable) FOnRoomEntered OnRoomEntered;
    UPROPERTY(BlueprintAssignable) FOnRoomExited  OnRoomExited;

    UBoxComponent* TriggerBox;
};

// OnBeginOverlap → Cast<AStudyBotCharacter> → SetCurrentRoom(Category, true)
// bAutoStartInterview=false 기본값 → E키 확인 단계 보장
```

---

## AStudyBotCharacter — Enhanced Input (UE 5.6)

```cpp
UCLASS()
class AStudyBotCharacter : public ACharacter
{
    // Enhanced Input (UE 5.1+ 공식, UE 5.6에서 기본)
    UPROPERTY(EditAnywhere) UInputMappingContext* IMC_StudyBot;
    UPROPERTY(EditAnywhere) UInputAction* IA_Move;
    UPROPERTY(EditAnywhere) UInputAction* IA_Look;
    UPROPERTY(EditAnywhere) UInputAction* IA_Interact;   // E키

    USpringArmComponent* SpringArm;
    UCameraComponent*    FollowCamera;

    ECardCategory m_currentRoomCategory = ECardCategory::All;
    bool          m_bInRoom = false;
};

// SetupPlayerInputComponent
EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ThisClass::OnInteract);

// OnInteract → m_bInRoom 확인 → OpenInterviewMap(m_currentRoomCategory)
```

**Enhanced Input 사용 이유:**
UE 5.6에서 Legacy Input System 완전 deprecated.
`UInputMappingContext`로 컨텍스트별 입력 전환(UI 오픈 시 이동 차단 등) 가능.

---

## ULobbyRoomWidget ★신규 — 로비 메인 화면

```cpp
UCLASS()
class ULobbyRoomWidget : public UUserWidget
{
    // Blueprint 바인딩
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtLobbyName;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtLobbyCode;   // "ABC123"
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtCategory;
    UPROPERTY(meta=(BindWidget)) UVerticalBox*     MemberList;     // 멤버 행 동적 추가
    UPROPERTY(meta=(BindWidget)) UScrollBox*       ChatScroll;
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputChat;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnSend;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnStart;       // Host만 표시
    UPROPERTY(meta=(BindWidget)) UButton*          BtnLeave;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtMessage;     // 시스템 메시지

    /** 멤버 행 Blueprint 위젯 클래스 (디자이너가 할당, 없으면 TextBlock 폴백) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSubclassOf<UUserWidget> MemberRowClass;

    void NativeConstruct() override;
    void NativeDestruct() override;
    void RefreshMemberList();
    void AppendChatMessage(const FString& Nickname, const FString& Message);

    // LobbySubsystem Delegate 구독
    UFUNCTION() void HandleMemberJoined    (const FLobbyMember& Member);
    UFUNCTION() void HandleMemberLeft      (int32 UserId);
    UFUNCTION() void HandleMemberKicked    (int32 UserId);
    UFUNCTION() void HandleChatReceived    (const FString& Nickname, const FString& Message);
    UFUNCTION() void HandleLobbyStarted   (const FString& Category, int32 CardCount);
    UFUNCTION() void HandleCategoryChanged (const FString& Category);
    UFUNCTION() void HandleLobbyError      (int32 Code, const FString& Message);
};
```

**BtnStart 가시성 조건:**
```cpp
void ULobbyRoomWidget::NativeConstruct()
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    // Host만 Visible, 멤버는 Collapsed (UI만, 실제 권한은 서버 sp_lobby_start가 재검증)
    BtnStart->SetVisibility(
        GI->IsHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
```

**HandleLobbyStarted — 카드 로딩 + 맵 이동:**
```cpp
void ULobbyRoomWidget::HandleLobbyStarted(const FString& Category, int32 CardCount)
{
    // ECardCategory 변환 후 카드 로딩 시작
    if (auto* Cards = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        Cards->FetchInterviewCards(ConvertCategory(Category), CardCount);

    Cast<UStudyBotGameInstance>(GetGameInstance())
        ->OpenInterviewMap(ConvertCategory(Category));
}
```

---

## UAlgorithmCardWidget — Algorithm 전용 카드

```cpp
UCLASS()
class UAlgorithmCardWidget : public UUserWidget
{
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* FaceSwitcher;   // 0=질문, 1=답변
    UPROPERTY(meta=(BindWidget)) UWidgetSwitcher* CodeSwitcher;   // 0=C++, 1=C#

    // 질문면
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtQuestion;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtCoreConditions;

    // 답변면
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtSelectionReason;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtAnswer;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtTimeComplexity;
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtCodeCpp;    // Consolas 폰트
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtCodeCSharp; // Consolas 폰트

    void SetCard(const FFlashCard& Card);
};
```

---

## UContributionWidget — 잔디 히트맵

```cpp
UCLASS()
class UContributionWidget : public UUserWidget
{
    UPROPERTY(meta=(BindWidget)) UUniformGridPanel* ContribGrid;  // 53×7

    TArray<FHeatmapEntry> m_allEntries;
    FString               m_filterCategory = "All";

    // 5단계 색상 (회색→연초록→중간→진초록→최진초록)
    static const TArray<FLinearColor> COLOR_STEPS;

    void BuildGrid(const TArray<FHeatmapEntry>& Entries);
    void OnCategoryChanged(FString NewCategory);
};
```

**BuildGrid — O(N) TMap 집계:**
```cpp
// 날짜→(CardsDone, KnownCount) TMap 집계 후 53×7 타일 생성
TMap<FString, TPair<int32,int32>> DayMap;
for (const auto& E : Entries) {
    if (m_filterCategory != "All" && E.Category != m_filterCategory) continue;
    auto& P = DayMap.FindOrAdd(E.ScoreDate);
    P.Key += E.CardsDone;
    P.Value += E.KnownCount;
}
// → 371개 UImage 타일에 COLOR_STEPS 색상 적용
```

---

## Widget 바인딩 이름 전체 목록

### UPreLobbyWidget
| 위젯 이름 | 타입 | 설명 |
|-----------|------|------|
| TxtNickname | UTextBlock | 로그인 닉네임 표시 |
| BtnCreate | UButton | WgtCreate 표시 |
| BtnJoin | UButton | WgtJoin 표시 |
| BtnLogout | UButton | 로그아웃 → LoginMap |
| WgtCreate | UUserWidget (Optional) | CreateLobbyWidget 오버레이 |
| WgtJoin | UUserWidget (Optional) | JoinLobbyWidget 오버레이 |

### UCreateLobbyWidget
| 위젯 이름 | 타입 | 설명 |
|-----------|------|------|
| InputName | UEditableTextBox | 로비 이름 |
| CbCategory | UComboBoxString | 면접장 선택 (Unreal/C++/CS/Company/Algorithm) |
| CbMaxMembers | UComboBoxString | 최대 인원 (2~6) |
| BtnCreate | UButton | POST /api/lobby |
| BtnBack | UButton | Collapsed으로 숨김 |
| TxtMessage | UTextBlock | 오류/상태 메시지 |

### UJoinLobbyWidget
| 위젯 이름 | 타입 | 설명 |
|-----------|------|------|
| InputCode | UEditableTextBox | 6자리 코드 입력 (자동 대문자) |
| BtnJoin | UButton | POST /api/lobby/join |
| BtnBack | UButton | Collapsed으로 숨김 |
| TxtMessage | UTextBlock | 오류/상태 메시지 |

### ULobbyRoomWidget
| 위젯 이름 | 타입 | 설명 |
|-----------|------|------|
| TxtLobbyName | UTextBlock | 로비 이름 |
| TxtLobbyCode | UTextBlock | 6자리 입장 코드 |
| TxtCategory | UTextBlock | 현재 면접장 카테고리 |
| MemberList | UVerticalBox | 멤버 행 동적 추가 (MemberRowClass 또는 TextBlock 폴백) |
| ChatScroll | UScrollBox | 채팅 메시지 스크롤 |
| InputChat | UEditableTextBox | 채팅 입력 |
| BtnSend | UButton | 채팅 전송 |
| BtnStart | UButton | 인터뷰 시작 (Host: Visible / Member: Collapsed) |
| BtnLeave | UButton | 로비 퇴장 → LeaveLobby() |
| TxtMessage | UTextBlock | 시스템 메시지 표시 |

---

## Build.cs — 모듈 의존성 (v3)

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine", "InputCore",
    "UMG",             // 위젯
    "HTTP",            // REST API
    "Json",            // JSON 파싱
    "JsonUtilities",
    "WebSockets",      // ULobbySubsystem WebSocket (★v3 신규)
    "Slate", "SlateCore",
});

PrivateDependencyModuleNames.AddRange(new string[]
{
    "RenderCore",
});
```

> **`EnhancedInput` 모듈 추가 시기:**
> `AStudyBotCharacter` Enhanced Input 기능을 구현할 때 `PublicDependencyModuleNames`에 `"EnhancedInput"` 추가. 현재는 미구현 상태이므로 Build.cs에서 제외.

**WebSockets 모듈 추가 이유:**
`FWebSocketsModule::Get().CreateWebSocket()`을 사용하려면 Build.cs에 명시 필요.
Android에서도 동작 확인된 UE 내장 WebSocket 구현입니다.

---

## DefaultGame.ini 설정

```ini
[StudyBot]
ServerBaseUrl=http://192.168.1.100:3000
WsBaseUrl=ws://192.168.1.100:3000
GoogleWebClientId=xxxx.apps.googleusercontent.com

; 에디터 테스트 시
; ServerBaseUrl=http://localhost:3000
; WsBaseUrl=ws://localhost:3000
```
