# StudyBot 문서 인덱스

> 최종 수정: 2026-04-17 · **v3** (C++ 백엔드 · UE 5.6 · 로비/Host 시스템 · Google JNI 로그인)

## 문서 목록

| 파일 | 내용 |
|------|------|
| [01_Architecture.md](01_Architecture.md) | 전체 구조 · 레이어 설계 이유 · Phase 1/2 로드맵 |
| [02_Backend.md](02_Backend.md) | C++ Drogon 서버 · Controller/Service/Repository · WebSocket 계층 |
| [03_UnrealEngine.md](03_UnrealEngine.md) | UE 5.6 클래스 계층 · LobbySubsystem · JNI Google 로그인 |
| [04_DataFlow.md](04_DataFlow.md) | 로그인 → 로비 → 인터뷰 전체 데이터 흐름 |
| [05_TestGuide.md](05_TestGuide.md) | 환경 구성 · Drogon 빌드 · API 테스트 · WebSocket 테스트 |
| [06_Flowcharts.md](06_Flowcharts.md) | 시스템 구조 · Google 로그인 · 로비 · WebSocket 이벤트 Mermaid 다이어그램 |
| [07_Android_Portfolio.md](07_Android_Portfolio.md) | Android 네이티브 연동 포트폴리오 — Google JNI · WebSocket · APL · 비즈니스 임팩트 |

---

## v3 기술 스택

| 영역 | 기술 | 선택 이유 |
|------|------|-----------|
| 클라이언트 | Unreal Engine 5.6 + Android | Enhanced Input, UE 5.6 공식 Android 지원 |
| 백엔드 | **C++ · Drogon 프레임워크** | HTTP + WebSocket 통합, MySQL 내장, C++20 코루틴 |
| 데이터베이스 | MySQL 8.0 + Stored Procedure | 쿼리 중앙화, SQL Injection 방어 |
| 인증 | JWT 24h + Google Sign-In (JNI) | 로컬/Google 계정 통합, Android SDK 직접 연동 |
| 실시간 통신 | **WebSocket (Drogon 내장)** | 로비 채팅·멤버 동기화·인터뷰 시작 신호 |
| Google 로그인 | play-services-auth + JNI Bridge | minSDK 24 호환, UE5 Android 네이티브 연동 |

---

## v3 주요 변경 사항

| 구분 | v2 (이전) | v3 (현재) |
|------|-----------|-----------|
| 백엔드 언어 | Node.js (Express) | **C++ (Drogon)** |
| UE 버전 | 5.x | **5.6** |
| Google 로그인 | 브라우저 런치 + 서버 폴링 | **JNI + Google Sign-In SDK** |
| 실시간 통신 | 없음 (REST 폴링) | **WebSocket** |
| 로비 시스템 | 없음 (직접 인터뷰 진입) | **Host 기반 로비 (생성·입장·채팅·시작)** |
| 맵 구조 | LoginMap → WorldMap → InterviewMap | **LoginMap → PreLobbyMap → LobbyRoomMap → InterviewMap** |
| 신규 UE 클래스 | — | `ULobbySubsystem`, `AStudyBotCharacter`, `UContributionWidget` 등 |
| 신규 DB 테이블 | — | `lobbies`, `lobby_members`, `lobby_invites`, `lobby_messages` |

---

## v3 신규 기능 요약

| 기능 | 관련 문서 |
|------|-----------|
| **Host 기반 로비** — 방 생성·코드 입장·멤버 관리·면접장 선택·시작 | 01, 02, 03, 04, 05, 06 |
| **WebSocket 실시간 채팅** — lobby_messages + WsSessionManager | 02, 04, 06 |
| **Google Sign-In JNI** — Android SDK + ID Token 서버 검증 | 02, 03, 04, 05, 06 |
| **C++ Drogon 서버** — Controller/Service/Repository/WsController 계층 | 02, 05 |
| **Algorithm 카테고리** — 핵심 조건·선택 이유·C++/C# 코드·시간복잡도 | 02, 03 |
| **잔디(Contribution Graph)** — daily_scores 누적 · UContributionWidget | 02, 03, 04 |

---

## 빠른 시작 (v3)

```bash
# ── 1. DB 초기화 ────────────────────────────────────────────
mysql -u root -p < server/db/schema.sql

# ── 2. 카드 데이터 삽입 ─────────────────────────────────────
pip install openpyxl mysql-connector-python
python scripts/create_algorithm_cards.py
python scripts/create_company_template.py
python scripts/import_cards.py --db

# ── 3. C++ 서버 빌드 ────────────────────────────────────────
# 사전 설치: Drogon, jwt-cpp, OpenSSL, libcurl, CMake 3.20+
mkdir -p server/build && cd server/build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# ── 4. 서버 설정 ────────────────────────────────────────────
cp server/config.json.example server/config.json
# config.json에서 db.password, jwt.secret, google.clientId 입력

# ── 5. 서버 실행 ────────────────────────────────────────────
./studybot_server   # → http://0.0.0.0:3000 / ws://0.0.0.0:3000

# ── 6. UE 5.6 프로젝트 열기 ─────────────────────────────────
# UnrealStudyBot.uproject 더블클릭
# Project Settings > Game Instance Class: StudyBotGameInstance
# Android Packaging > Min SDK: 24, Target SDK: 34
# Android > Google Services > google-services.json 배치

# ── 7. Android 빌드 ─────────────────────────────────────────
# File > Package Project > Android (ASTC)
```

---

## 폴더 구조

```
StudyBot/
├── server/                   ← C++ Drogon 백엔드 (v3 신규)
│   ├── CMakeLists.txt
│   ├── config.json.example
│   ├── main.cpp
│   ├── controllers/          ← AuthController, LobbyController, LobbyWsController
│   ├── services/             ← AuthService, LobbyService, CardService
│   ├── repositories/         ← AuthRepository, LobbyRepository, CardRepository
│   ├── websocket/            ← WsSessionManager
│   ├── middleware/           ← JwtFilter
│   ├── models/               ← Dto 구조체
│   └── db/
│       └── schema.sql        ← 전체 테이블 + Stored Procedure
├── UnrealStudyBot/Source/UnrealStudyBot/    ← UE 5.6 C++ 소스
│   ├── Models/StudyBotTypes.h
│   ├── UnrealStudyBotGameInstance.h/.cpp
│   ├── Subsystem/
│   │   ├── AuthSubsystem.h/.cpp        ← Google JNI 추가
│   │   ├── CardSubsystem.h/.cpp
│   │   └── LobbySubsystem.h/.cpp       ← v3 신규
│   ├── World/
│   │   └── RoomTriggerActor.h/.cpp
│   ├── Character/
│   │   └── StudyBotCharacter.h/.cpp
│   └── UI/
│       ├── LoginWidget.h/.cpp          ← BtnGoogle 추가
│       ├── PreLobbyWidget.h/.cpp       ← v3 신규
│       ├── CreateLobbyWidget.h/.cpp    ← v3 신규
│       ├── JoinLobbyWidget.h/.cpp      ← v3 신규
│       ├── LobbyRoomWidget.h/.cpp      ← v3 신규
│       ├── InterviewWidget.h/.cpp
│       ├── AlgorithmCardWidget.h/.cpp
│       └── ContributionWidget.h/.cpp
├── card/                     ← 원본 카드 데이터 (xlsx/csv)
├── scripts/                  ← Python 카드 분류·생성 스크립트
└── docs/                     ← 이 문서들
```

---

## Phase 로드맵

### Phase 1 — 현재 목표

- [x] C++ Drogon 서버로 전환 (기존 API 포팅)
- [x] Google JNI 로그인 (AuthSubsystem + GoogleAuthHelper)
- [x] 로비 생성 · 코드 입장 (LobbySubsystem REST)
- [x] WebSocket 실시간 채팅·멤버 동기화
- [x] Host 면접장 선택 · 인터뷰 시작 신호
- [x] 멤버 강퇴 · 나가기
- [x] Algorithm/Company/잔디 기능 유지
- [x] UE5 위젯 구현 (PreLobby·CreateLobby·JoinLobby·LobbyRoom)

### Phase 2 — 다음 목표

- [ ] Ready 상태 동기화
- [ ] Host 권한 위임
- [ ] Reconnect + Heartbeat
- [ ] Push 알림 (FCM)
- [ ] 음성 채팅 (Vivox / Agora)
- [ ] UE5 Listen Server 멀티플레이 전환
