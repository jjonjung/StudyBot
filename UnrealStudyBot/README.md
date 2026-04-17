# UnrealStudyBot — NPC 면접 플래시카드 (UE5 Android)

## 구조

```
UnrealStudyBot/
├── Source/UnrealStudyBot/
│   ├── Models/StudyBotTypes.h          # 공용 구조체/열거형
│   ├── UnrealStudyBotGameInstance      # 전역 상태(토큰, URL, 씬이동)
│   ├── Subsystem/
│   │   ├── AuthSubsystem               # 로그인/회원가입 HTTP
│   │   └── CardSubsystem               # 카드 조회/진도 저장 HTTP
│   ├── NPC/
│   │   ├── NPCDialogueComponent        # 질문 순서 제어, NPC 멘트
│   │   └── InterviewNPCActor           # NPC 캐릭터 (Blueprint 확장)
│   └── UI/
│       ├── LoginWidget                 # 로그인/회원가입 화면
│       ├── CategorySelectWidget        # 로비 (카테고리 선택)
│       ├── FlashcardWidget             # 플립 카드
│       └── InterviewWidget             # NPC 인터뷰 메인 화면
└── Config/
    ├── DefaultGame.ini                 # ServerBaseUrl 설정
    └── DefaultEngine.ini               # Android 빌드 설정
```

## 화면 흐름

```
LoginMap  →  LobbyMap  →  InterviewMap
  (로그인)    (카테고리)    (NPC + 카드)
                              ↓
                         결과 화면 (재도전 / 로비)
```

## Blueprint 설정 체크리스트

### 1. GameInstance 설정
- Project Settings > Maps & Modes > Game Instance Class = `StudyBotGameInstance`

### 2. BP_LoginWidget
- 부모 클래스: `LoginWidget`
- 바인딩: `InputUsername`, `InputPassword`, `InputNickname`, `BtnLogin`, `BtnRegister`, `TxtMessage`

### 3. BP_FlashcardWidget
- 부모 클래스: `FlashcardWidget`
- 바인딩: `CardSwitcher`(WidgetSwitcher), `TxtQuestion`, `TxtAnswer`, `TxtCategory`, `TxtProgress`, `BtnFlip`, `BtnKnown`, `BtnUnknown`

### 4. BP_InterviewWidget
- 부모 클래스: `InterviewWidget`
- 바인딩: `ScreenSwitcher`(WidgetSwitcher), `TxtNPCSpeech`, `TxtNPCComment`, `CardWidget`(FlashcardWidget), `TxtResultScore`, `TxtResultCategory`, `BtnRetry`, `BtnLobby`

### 5. BP_InterviewNPC
- 부모 클래스: `InterviewNPCActor`
- `OnNPCSpeaks` 이벤트에서 말풍선 위젯 업데이트

### 6. InterviewMap 레벨 Blueprint
```
BeginPlay
  → GetGameInstance → Cast<StudyBotGameInstance>
  → GetSubsystem<CardSubsystem>
  → BindEvent OnCardsLoaded → InterviewWidget::StartInterview
  → FetchInterviewCards(category, 10)
```

## 서버 설정

`Config/DefaultGame.ini`에서 서버 주소 변경:
```ini
[StudyBot]
ServerBaseUrl=http://YOUR_SERVER_IP:3000
```
> 에뮬레이터에서 PC 로컬 서버: `10.0.2.2:3000`

## Android 빌드 순서

1. Android SDK/NDK 설치 (UE5 요구 버전 확인)
2. `Project Settings > Android` — SDK 경로 설정
3. `File > Package Project > Android (ASTC)`
4. 생성된 `.apk` 단말기에 설치
