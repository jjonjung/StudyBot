# 05. 테스트 가이드 (v3)

## 목차

1. [환경 구성](#1-환경-구성)
   - [WSL 설치 (Windows)](#wsl-설치-windows-11-기준)
   - [Drogon 설치](#drogon-설치-ubuntuwindows-wsl)
2. [C++ 서버 빌드 및 실행](#2-c-서버-빌드-및-실행)
3. [DB 초기화 및 카드 데이터 삽입](#3-db-초기화-및-카드-데이터-삽입)
4. [백엔드 API 테스트](#4-백엔드-api-테스트)
5. [WebSocket 테스트](#5-websocket-테스트)
6. [Google 로그인 설정](#6-google-로그인-설정)
7. [UE 5.6 에디터 테스트](#7-ue-56-에디터-테스트)
8. [Android 빌드 및 실기기 테스트](#8-android-빌드-및-실기기-테스트)
9. [오류 상황별 대처](#9-오류-상황별-대처)

---

## 1. 환경 구성

### 필수 설치

| 항목 | 버전 | 용도 |
|------|------|------|
| Unreal Engine | 5.6 이상 | 클라이언트 |
| CMake | 3.20 이상 | C++ 서버 빌드 |
| Drogon | 최신 | C++ 웹 프레임워크 |
| jwt-cpp | 최신 | JWT 발급·검증 |
| OpenSSL | 3.x | HTTPS (Google tokeninfo) |
| libcurl | 최신 | Google API 호출 |
| MySQL | 8.0 이상 | DB |
| Python | 3.10 이상 | 카드 스크립트 |
| Android Studio | 최신 안정 | SDK/NDK 관리 |

### Android SDK/NDK (UE 5.6 기준)

| 항목 | 버전 |
|------|------|
| Android SDK | API 34 |
| NDK | r25c 이상 |
| JDK | 17 |
| Gradle | 8.x |
| Min SDK | 24 (Android 7.0) |

### WSL 설치 (Windows 11 기준)

> **실행 환경: PowerShell — 관리자 권한으로 실행**
> 시작 메뉴에서 "PowerShell"을 우클릭 → **관리자 권한으로 실행** 선택 후 아래 명령을 입력하세요.

```powershell
# WSL 및 Ubuntu 설치 (재부팅 필요)
wsl --install

# 재부팅 후 Ubuntu 앱이 자동으로 열리면
# 사용할 Linux 사용자 이름과 비밀번호를 설정합니다
```

이후 시작 메뉴에서 **Ubuntu** 앱을 열거나, CMD/PowerShell에서 `wsl`을 입력하면
**WSL bash 터미널**로 진입할 수 있습니다.

> **참고**: 아래 Drogon 설치부터는 모두 이 WSL bash 터미널에서 실행합니다.

---

### Drogon 설치 (Ubuntu/Windows WSL)

> **실행 환경: WSL 터미널 (bash)**
> Windows의 CMD나 PowerShell이 아닌, WSL 안의 bash 셸에서 실행하세요.

```bash
# 의존성
sudo apt install -y cmake libssl-dev libcurl4-openssl-dev libjsoncpp-dev
sudo apt install -y libmysqlclient-dev

# Drogon 빌드
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_MYSQL=ON
make -j$(nproc) && sudo make install

# jwt-cpp
git clone https://github.com/Thalhammer/jwt-cpp
cd jwt-cpp && mkdir build && cd build
cmake .. && sudo make install
```

---

## 2. C++ 서버 빌드 및 실행

### 빌드

> **실행 환경: WSL (Windows Subsystem for Linux) 터미널**
> CMD나 PowerShell에서는 `sudo`, `make`, `nproc` 같은 명령어가 동작하지 않습니다.
> 반드시 WSL bash 셸 안에서 실행하세요.

```bash
# WSL 또는 Linux 환경에서 실행
# Windows 경로(/mnt/c/...)로 접근하거나, WSL 내부 경로에 복사 후 빌드 권장

# WSL에서 Windows 드라이브 접근 시
cd /mnt/c/Users/EJ/Desktop/Fork/StudyBot/server

# 또는 WSL 홈 디렉터리에 복사 후 빌드 (I/O 성능 향상)
cp -r /mnt/c/Users/EJ/Desktop/Fork/StudyBot/server ~/studybot_server
cd ~/studybot_server

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

> **주의**: `server/` 폴더는 C++ Drogon 서버 소스 작성 후 생성됩니다.
> 현재 `backend/`는 v2 Node.js 서버입니다 (v3에서 대체 예정).

### 설정 파일

```json
// server/config.json
{
  "listeners": [{"address":"0.0.0.0","port":3000}],
  "db_clients": [{
    "rdbms": "mysql",
    "host": "127.0.0.1",
    "port": 3306,
    "dbname": "studybot",
    "user": "root",
    "passwd": "your_password",
    "connection_number": 10
  }],
  "app": {
    "jwt_secret": "your-256-bit-secret",
    "google_client_id": "xxxx.apps.googleusercontent.com",
    "bcrypt_cost": 10
  }
}
```

### 실행 및 확인

> **실행 환경: WSL 터미널**

```bash
./studybot_server
# → Drogon v1.x.x   The IOLoop 시작 메시지

# 상태 확인
curl http://localhost:3000/health
# → {"status":"ok","version":"3.0.0"}
```

---

## 3. DB 초기화 및 카드 데이터 삽입

```bash
# StudyBot 루트에서 실행
# DB 스키마 적용 (테이블 + 저장 프로시저 전체)
mysql -u root -p < server/db/schema.sql
# server/ 폴더 생성 전이라면:
# mysql -u root -p < backend/db/schema.sql  (v2 Node.js 스키마 참고용)

# 카드 스크립트 의존성
pip install openpyxl mysql-connector-python

# Algorithm 카드 생성
python scripts/create_algorithm_cards.py

# Company (기업 기출) 카드 생성
python scripts/create_company_template.py

# 전체 카드 분류 + DB 삽입
python scripts/import_cards.py --db
```

### DB 삽입 확인

```sql
USE studybot;
SELECT category, COUNT(*) AS cnt FROM flashcards GROUP BY category;
-- Unreal    | N
-- C++       | N
-- CS        | N
-- Company   | N
-- Algorithm | N

-- 로비 테이블 구조 확인
SHOW TABLES;
-- lobbies, lobby_members, lobby_invites, lobby_messages 포함 확인
```

---

## 4. 백엔드 API 테스트

### 4-1. 인증

```bash
# 회원가입
curl -X POST http://localhost:3000/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"host1","password":"test1234","nickname":"호스트"}'
# → {"id":1,"message":"registered"}

# 로그인
curl -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"host1","password":"test1234"}'
# → {"token":"eyJ...","nickname":"호스트","userId":1}

TOKEN_HOST="eyJ..."

# 두 번째 테스트 사용자 (Member용)
curl -X POST http://localhost:3000/api/auth/register \
  -d '{"username":"member1","password":"test1234","nickname":"멤버"}' \
  -H "Content-Type: application/json"
curl -X POST http://localhost:3000/api/auth/login \
  -d '{"username":"member1","password":"test1234"}' \
  -H "Content-Type: application/json"
TOKEN_MEMBER="eyJ..."
```

### 4-2. 로비 생성

```bash
curl -X POST http://localhost:3000/api/lobby \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_HOST" \
  -d '{"name":"스터디룸A","category":"Algorithm","maxMembers":4}'
# → {"lobbyId":1,"code":"ABC123","name":"스터디룸A","category":"Algorithm","maxMembers":4}

LOBBY_ID=1
LOBBY_CODE="ABC123"
```

### 4-3. 코드로 입장

```bash
curl -X POST http://localhost:3000/api/lobby/join \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_MEMBER" \
  -d "{\"code\":\"$LOBBY_CODE\"}"
# → { lobbyId, code, name, category, members:[{userId,nickname,role,...},...] }
```

### 4-4. 로비 정보 조회

```bash
curl "http://localhost:3000/api/lobby/$LOBBY_ID" \
  -H "Authorization: Bearer $TOKEN_HOST"
# → 로비 + 멤버 전체 목록
```

### 4-5. 멤버 강퇴

```bash
curl -X POST "http://localhost:3000/api/lobby/$LOBBY_ID/kick" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_HOST" \
  -d '{"targetUserId":2}'
# → {"message":"kicked"}

# Host가 아닌 멤버로 강퇴 시도 (실패해야 함)
curl -X POST "http://localhost:3000/api/lobby/$LOBBY_ID/kick" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_MEMBER" \
  -d '{"targetUserId":1}'
# → 403 {"error":"NOT_HOST"}
```

### 4-6. 카드 조회

```bash
# Algorithm 인터뷰용 랜덤 10장
curl "http://localhost:3000/api/cards/interview?category=Algorithm&count=10" \
  -H "Authorization: Bearer $TOKEN_HOST"
# → core_conditions, code_cpp 등 포함 확인

# 기업 목록
curl http://localhost:3000/api/cards/companies \
  -H "Authorization: Bearer $TOKEN_HOST"
```

### 4-7. 진도 저장 + 잔디

```bash
# 카드 진도 저장 (UPSERT)
curl -X PUT http://localhost:3000/api/progress/1 \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_HOST" \
  -d '{"known":1,"score":5}'

# 세션 저장 (interview_sessions + daily_scores 트랜잭션)
curl -X POST http://localhost:3000/api/progress/session \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_HOST" \
  -d '{"category":"Algorithm","total_cards":10,"known_count":7}'

# 잔디 데이터 조회
curl "http://localhost:3000/api/progress/heatmap?year=2025" \
  -H "Authorization: Bearer $TOKEN_HOST"
# → [{ score_date, category, cards_done, known_count, ratio }]
```

---

## 5. WebSocket 테스트

### wscat 또는 websocat 사용

```bash
# websocat 설치 (Linux/WSL)
cargo install websocat

# Host WebSocket 연결
websocat "ws://localhost:3000/ws/lobby/$LOBBY_ID?token=$TOKEN_HOST"

# Member WebSocket 연결 (다른 터미널)
websocat "ws://localhost:3000/ws/lobby/$LOBBY_ID?token=$TOKEN_MEMBER"
```

### 이벤트 시나리오 테스트 순서

```bash
# 1. Member 연결 → Host 창에서 수신 확인
# Member가 연결하면 Host 창에:
# {"event":"member/joined","payload":{"userId":2,"nickname":"멤버","role":"member"}}

# 2. 채팅 전송 (Host 창에서 입력)
{"event":"chat/send","payload":{"message":"안녕하세요"}}
# 양쪽 창 모두에서 수신:
# {"event":"chat/message","payload":{"userId":1,"nickname":"호스트","message":"안녕하세요",...}}

# 3. 면접장 변경 (Host만 가능)
{"event":"lobby/set_category","payload":{"category":"Unreal"}}
# 양쪽에서: {"event":"lobby/category_changed","payload":{"category":"Unreal"}}

# 4. 인터뷰 시작 (Host만 가능)
{"event":"lobby/start","payload":{}}
# 양쪽에서: {"event":"lobby/started","payload":{"category":"Unreal","cardCount":10}}

# 5. Member가 시작 시도 (실패해야 함)
# Member 창에서:
{"event":"lobby/start","payload":{}}
# Member 창에서만: {"event":"error","payload":{"message":"권한이 없습니다."}}

# 6. 연결 종료 → 상대방 창에서 수신 확인
# {"event":"member/left","payload":{"userId":2,"nickname":"멤버"}}
```

---

## 6. Google 로그인 설정

### Google Cloud Console 설정

```
1. console.cloud.google.com → 새 프로젝트 생성 또는 기존 프로젝트 선택
2. APIs & Services → OAuth consent screen → External 설정
3. APIs & Services → Credentials → Create Credential

   ① Web application (서버 측 토큰 검증용)
      → Web Client ID 생성
      → Authorized redirect URIs: 불필요 (tokeninfo API 방식)

   ② Android (앱 JNI 연동용)
      → Package name: com.studybot.game  ← DefaultEngine.ini PackageName과 반드시 일치
      → SHA-1: 아래 명령으로 확인

# SHA-1 fingerprint 확인
keytool -list -v -keystore debug.keystore -alias androiddebugkey \
        -storepass android -keypass android
# 배포용: 배포 keystore의 SHA-1로 별도 등록
```

### APL 파일 — Java 소스 등록 (자동 처리)

`Source/UnrealStudyBot/UnrealStudyBot_APL.xml` 이 UBT에 의해 자동 인식되어
아래 작업을 빌드 시 자동으로 처리합니다. **별도 편집 불필요.**

```
처리 내용:
  • play-services-auth:21.2.0 Gradle 의존성 주입
  • Java/GoogleAuthHelper.java → Gradle 소스셋 복사
  • GameActivity.onActivityResult() 에 결과 처리 코드 삽입
  • AndroidManifest에 networkSecurityConfig 속성 추가
```

### google-services.json 배치

```
Google Cloud Console에서 다운로드 후:
  UnrealStudyBot/Build/Android/google-services.json

APL의 apply plugin: 'com.google.gms.google-services' 가 이 파일을 처리합니다.
```

### 로컬 테스트 (Google 로그인 모의)

실제 Android 기기 없이 서버 측만 테스트:

```bash
# Google tokeninfo 응답을 모방한 더미 테스트
# 실제 idToken은 Android 기기에서만 발급 가능
# 서버 테스트용: 환경변수로 검증 스킵 설정

# config.json에 테스트 모드 추가
"google_skip_verify": false   # 프로덕션은 반드시 false

# 테스트용 Mock 엔드포인트 (개발 환경만)
curl -X POST http://localhost:3000/api/auth/google/mobile \
  -H "Content-Type: application/json" \
  -d '{"idToken":"mock-valid-token-for-dev"}'
```

---

## 7. UE 5.6 에디터 테스트

### 7-1. 프로젝트 초기 설정

```
Project Settings
  → Maps & Modes
     → Game Instance Class: StudyBotGameInstance
     → Default Map: LoginMap

  → Android
     → Min SDK Version: 24
     → Target SDK Version: 34
     → Build.cs: "WebSockets" 모듈 포함 확인
```

### 7-2. Blueprint 설정 체크리스트

**LoginMap — BP_LoginMap_LevelBlueprint**
```
Event BeginPlay
  → Create Widget (BP_LoginWidget) → Add to Viewport
```

**BP_LoginWidget 생성**
```
부모 클래스: LoginWidget (C++)
바인딩 위젯 (BindWidget — 필수):
  InputUsername, InputPassword, InputNickname, BtnLogin, BtnRegister, TxtMessage

바인딩 위젯 (BindWidgetOptional — 선택):
  BtnGoogle ★신규 (Android 전용, PC 에디터에서는 없어도 무방)
```

**PreLobbyMap — BP_PreLobbyWidget**
```
부모: PreLobbyWidget (C++)
바인딩: TxtNickname, BtnCreate, BtnJoin, BtnLogout
         WgtCreate (Optional), WgtJoin (Optional)
```

**BP_CreateLobbyWidget**
```
부모: CreateLobbyWidget (C++)
바인딩: InputName, CbCategory, CbMaxMembers, BtnCreate, BtnBack, TxtMessage
```

**BP_JoinLobbyWidget**
```
부모: JoinLobbyWidget (C++)
바인딩: InputCode, BtnJoin, BtnBack, TxtMessage
```

**LobbyRoomMap — BP_LobbyRoomWidget ★신규**
```
부모: LobbyRoomWidget (C++)
바인딩: TxtLobbyName, TxtLobbyCode, TxtCategory,
        MemberList (VerticalBox), ChatScroll (ScrollBox), InputChat,
        BtnSend, BtnStart, BtnLeave, TxtMessage
세부: MemberRowClass → 멤버 행 Blueprint 클래스 지정 (없으면 TextBlock 폴백)
```

**InterviewMap Level Blueprint**
```
Event BeginPlay
  → Get Game Instance → Cast to StudyBotGameInstance
  → Get Selected Category
  → Get Subsystem (CardSubsystem)
  → Bind Event to OnCardsLoaded → InterviewWidget::StartInterview
  → Fetch Interview Cards (Category=selected, Count=10)
```

### 7-3. DefaultGame.ini 설정

```ini
[StudyBot]
ServerBaseUrl=http://localhost:3000
WsBaseUrl=ws://localhost:3000
```

> Google Web Client ID는 INI가 아닌 `Build/Android/res/values/google_strings.xml`의
> `google_web_client_id` 리소스로 관리됩니다. Android 빌드에서만 참조됩니다.

### 7-4. PIE 테스트 순서

```
1. Play In Editor 실행 (▶)

2. LoginMap
   □ 로컬 로그인: 아이디/비밀번호 입력 → 로그인 성공 → PreLobbyMap 이동
   □ Output Log에서 HTTP 200 확인

3. PreLobbyMap
   □ 닉네임 표시 확인
   □ BtnCreate → CreateLobbyWidget 표시
   □ 로비 생성 → LobbyRoomMap 이동
   □ 로비 코드 표시 확인 ("초대 코드: ABC123")

4. LobbyRoomMap (두 번째 PIE 창 또는 다른 기기)
   □ 코드 입력 → 입장 → 멤버 목록에 추가 확인
   □ 채팅 메시지 양방향 전달 확인
   □ Host에서 카테고리 변경 → 양쪽 동기화 확인
   □ Host BtnStart → 양쪽 InterviewMap 이동

5. InterviewMap
   □ NPC 질문 표시
   □ Algorithm 카드: CodeSwitcher (C++/C#) 전환 확인
   □ 알았어요/몰랐어요 → 다음 카드
   □ 10문제 후 결과 화면
   □ DB: SELECT * FROM interview_sessions; 세션 저장 확인
   □ DB: SELECT * FROM daily_scores; 잔디 누적 확인
```

### 7-5. Output Log 확인 포인트

```
LogHttp: POST http://localhost:3000/api/auth/login → 200
LogHttp: POST http://localhost:3000/api/lobby → 201
LogWebSocket: Connected to ws://localhost:3000/ws/lobby/1
LogWebSocket: Received {"event":"member/joined",...}
LogHttp: PUT http://localhost:3000/api/progress/1 → 200
```

---

## 8. Android 빌드 및 실기기 테스트

### 8-1. 서버 주소 변경

```ini
# Config/DefaultGame.ini
[StudyBot]
ServerBaseUrl=http://192.168.1.100:3000   ← PC 실제 IP
WsBaseUrl=ws://192.168.1.100:3000
```

Windows 방화벽에서 3000번 포트 인바운드 허용:
```powershell
netsh advfirewall firewall add rule name="StudyBot" dir=in action=allow protocol=TCP localport=3000
```

### 8-2. Android 빌드

```
UE 5.6 에디터 → File → Package Project → Android → Android (ASTC)
→ 출력 폴더 지정
→ 빌드 완료 후 Android/APK/*.apk 생성
```

### 8-3. 기기 설치

```bash
# 개발자 옵션 + USB 디버깅 활성화 필요
adb install -r StudyBot.apk

# 또는 패키징 출력 폴더의 Install_*.bat 실행
```

### 8-4. 실기기 테스트 체크리스트

```
□ 앱 실행 → LoginMap 표시
□ 로컬 로그인 → 성공 → PreLobbyMap 이동
□ Google 로그인 → 계정 선택 팝업 → 성공 → PreLobbyMap
□ 방 만들기 → 로비 생성 → LobbyRoomMap
□ 코드 표시 확인, 다른 기기로 입장
□ 채팅 실시간 수신 확인
□ Host 카테고리 변경 → 다른 기기 동기화
□ Host 인터뷰 시작 → 양쪽 InterviewMap 이동
□ Algorithm 카드: C++/C# 코드 탭 전환
□ 10문제 후 결과 확인
□ DB 세션·잔디 저장 확인
□ 앱 백그라운드 전환 후 복귀 → 연결 유지 (또는 재연결 메시지)
```

---

## 9. 오류 상황별 대처

### 서버 빌드 오류

| 증상 | 원인 | 해결 |
|------|------|------|
| `drogon not found` | Drogon 미설치 | Drogon 빌드 및 설치 재시도 |
| `jwt.h not found` | jwt-cpp 미설치 | jwt-cpp CMake install |
| `mysqlclient not found` | MySQL 개발 헤더 없음 | `apt install libmysqlclient-dev` |
| `Cannot find package Drogon` | CMake 경로 문제 | `cmake .. -DDrogon_DIR=/usr/local/lib/cmake/Drogon` |

### 서버 런타임 오류

| 증상 | 원인 | 해결 |
|------|------|------|
| `ER_ACCESS_DENIED_ERROR` | DB 비밀번호 틀림 | `config.json` `passwd` 확인 |
| `ECONNREFUSED 3306` | MySQL 미실행 | `net start MySQL80` (Windows CMD) |
| `LOBBY_NOT_FOUND` | 잘못된 코드 입력 | 클라이언트 입력값 확인 |
| `NOT_HOST` | 권한 없는 액션 시도 | 정상 동작 (Member → Host 시도 차단) |
| `jwt expired` | JWT 24h 만료 | 재로그인 유도 |

### UE5 빌드 오류

| 증상 | 원인 | 해결 |
|------|------|------|
| `IWebSocketsModule not found` | Build.cs에 "WebSockets" 누락 | `PublicDependencyModuleNames`에 추가 |
| `BindWidget not connected` | 위젯 이름 불일치 | C++ 변수명 = Blueprint Designer 이름 |
| `InputNickname BindWidget 오류` | LoginWidget에 InputNickname 미배치 | BP 디자이너에서 EditableTextBox 이름 확인 |
| `Cast failed GameInstance` | GameInstance 미등록 | Project Settings > Game Instance 재확인 |
| `JNI: class not found` | Java 소스 미등록 | `UnrealStudyBot_APL.xml` resourceCopies 확인 |

### Android 오류

| 증상 | 원인 | 해결 |
|------|------|------|
| Google 로그인 팝업 안 뜸 | SHA-1 불일치 | debug.keystore SHA-1로 Cloud Console 재등록 |
| `google-services.json 없음` | 파일 미배치 | Build/Android/ 폴더에 배치 |
| WebSocket 연결 실패 | HTTP Cleartext 차단 | `network_security_config.xml` 개발 환경 허용 or WSS 전환 |
| 서버 연결 실패 | IP 주소 오류 | `ipconfig`로 PC IP 재확인 |
| 한글 깨짐 | 폰트 미설정 | UMG에서 Noto Sans KR 등 지정 |

### 카드 스크립트 오류

| 증상 | 원인 | 해결 |
|------|------|------|
| `FileNotFoundError` | 경로 오류 | `StudyBot/` 루트에서 실행 |
| `UnicodeDecodeError` | CSV 인코딩 | `CS_260325.csv`가 CP949인지 확인 |
| `ModuleNotFoundError` | pip 미설치 | `pip install openpyxl mysql-connector-python` |

### DB 검증 쿼리

```sql
USE studybot;

-- 카드 분포 확인
SELECT category, COUNT(*) FROM flashcards GROUP BY category;

-- 로비 상태 확인
SELECT id, code, name, category, status, host_user_id FROM lobbies;

-- 멤버 목록 확인
SELECT lm.lobby_id, u.nickname, lm.role, lm.is_ready
  FROM lobby_members lm JOIN users u ON u.id = lm.user_id;

-- 채팅 이력 확인
SELECT lm.lobby_id, u.nickname, lm.message, lm.sent_at
  FROM lobby_messages lm JOIN users u ON u.id = lm.user_id
 ORDER BY lm.sent_at DESC LIMIT 20;

-- 잔디 확인
SELECT user_id, category, score_date, cards_done, known_count
  FROM daily_scores ORDER BY score_date DESC LIMIT 10;

-- 세션 이력 확인
SELECT * FROM interview_sessions ORDER BY played_at DESC LIMIT 10;
```
