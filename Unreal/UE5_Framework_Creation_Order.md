# UE5 프레임워크 생성 순서

> 직접 로그 추적으로 확인한 UE5 GameFramework 클래스 생성 및 호출 순서  
> 싱글플레이(에디터 PIE) 기준

---

## 전체 흐름 요약

```
CDO 생성 (엔진 초기화)
  → GameMode 생성 → InitGame → GameState 생성
    → Login
      → PlayerController 생성 → PlayerState 생성
        → PostInitializeComponents → InitPlayerState
          → SetupInputComponent → ReceivedPlayer
            → PostLogin (Super 내부)
              → HandleStartingNewPlayer
                → Pawn 스폰 → PossessedBy
                  → AcknowledgePossession → OnPossess
            → PostLogin (로그)
              → BeginPlay (GameMode → GameState → PC → PS → Pawn)
                → StartPlay
```

---

## 구간별 상세

### 1. CDO 생성 (엔진 초기화 단계)

| 호출 | 설명 |
|------|------|
| `ALabGameMode::ALabGameMode` | CDO 생성 |
| `ALabGameState::ALabGameState` | CDO 생성 |
| `ALabPawn::ALabPawn` | CDO 생성 |
| `ALabPlayerController::ALabPlayerController` | CDO 생성 |
| `ALabPlayerState::ALabPlayerState` | CDO 생성 |

- 게임 시작 전 엔진이 각 클래스의 **Class Default Object(CDO)** 를 미리 생성
- 생성자는 이 시점에 한 번 호출됨
- **실제 게임 인스턴스가 아님** — 기본값 설정 용도

---

### 2. 게임 시작

| 호출 | 설명 |
|------|------|
| `ALabGameMode::ALabGameMode` | 실제 인스턴스 생성 |
| `ALabGameMode::InitGame` | 게임 초기화, GameState 생성 트리거 |
| `ALabGameState::ALabGameState` | 실제 인스턴스 생성 |

- **PlayerController보다 GameMode/GameState가 먼저 존재**
- `InitGame`은 맵 로딩 직후, 플레이어 접속 이전에 호출됨
- 게임 전역 설정(라운드 제한, 규칙 등) 초기화에 적합한 위치

---

### 3. 플레이어 접속

| 호출 | 설명 |
|------|------|
| `ALabGameMode::Login` | 접속 허가/거부 결정, PlayerController 생성 반환 |
| `ALabPlayerController::ALabPlayerController` | PC 실제 인스턴스 생성 |
| `ALabPlayerState::ALabPlayerState` | PS 실제 인스턴스 생성 |
| `ALabPlayerState::PostInitializeComponents` | PS 컴포넌트 초기화 완료 |
| `ALabPlayerController::InitPlayerState` | PC에 PS를 연결 |
| `ALabPlayerController::SetupInputComponent` | 입력 바인딩 설정 |
| `ALabPlayerController::ReceivedPlayer` | 로컬 플레이어 연결 완료 |

- `Login`의 반환값이 곧 생성된 PlayerController
- `InitPlayerState` 이후부터 `PC->PlayerState` 참조 가능

---

### 4. Pawn 스폰 & Possess

| 호출 | 설명 |
|------|------|
| `ALabGameMode::HandleStartingNewPlayer_Implementation` | Pawn 스폰 트리거 (Super 내부에서 RestartPlayer 호출) |
| `ALabPawn::ALabPawn` | Pawn 인스턴스 생성 |
| `ALabPawn::PostInitializeComponents` | Pawn 컴포넌트 초기화 완료 |
| `ALabPawn::PossessedBy` | Pawn 입장 — "내가 빙의당했다" |
| `ALabPlayerController::AcknowledgePossession` | 클라이언트 측 Possess 확인 |
| `ALabPlayerController::OnPossess` | PC 입장 — "내가 빙의했다" |

- `HandleStartingNewPlayer`는 `PostLogin`의 **Super 내부**에서 호출됨
- `Super::HandleStartingNewPlayer` 안에서 `RestartPlayer` → Pawn 스폰 → Possess 순으로 진행
- **`AcknowledgePossession`은 클라이언트 전용** — 싱글플레이에서는 같은 프로세스라 함께 찍히지만, 멀티플레이에서는 다른 머신에서 실행됨

---

### 5. PostLogin

| 호출 | 설명 |
|------|------|
| `ALabGameMode::PostLogin` | Super 완료 후 로그 — 모든 준비가 끝난 마지막 훅 |

- `PostLogin` 내부 흐름:
  ```
  PostLogin 진입
    → Super::PostLogin
      → HandleStartingNewPlayer → Pawn 스폰 → Possess
    → FRAMEWORK_LOG() (Super 이후)
  ```
- **PostLogin 로그 시점에는 PC, PS, Pawn 모두 준비 완료** → 게임 로직 초기화 넣기 가장 안전한 위치

> ⚠️ 로그 위치 = 실행 시점  
> `Super::` 호출 전/후 어디에 로그를 찍느냐에 따라 순서가 완전히 다르게 보임.  
> 흐름 추적 시 항상 **Super 전에 로그**를 찍는 것이 실제 진입 시점을 반영함.

---

### 6. BeginPlay

| 호출 순서 | 클래스 |
|-----------|--------|
| 1 | `ALabGameMode::BeginPlay` |
| 2 | `ALabGameState::BeginPlay` |
| 3 | `ALabPlayerController::BeginPlay` |
| 4 | `ALabPlayerState::BeginPlay` |
| 5 | `ALabPawn::BeginPlay` |
| 6 | `ALabGameMode::StartPlay` |

- `StartPlay`가 월드의 BeginPlay들을 순차 트리거하는 주체
- BeginPlay 시점에는 월드의 모든 액터가 이미 존재 → **다른 액터 참조 안전**
- `StartPlay`는 BeginPlay들이 모두 끝난 뒤 "게임 시작" 선언

---

## 네트워크 관점 클래스 존재 위치

| 클래스 | 서버 | 해당 클라이언트 | 다른 클라이언트 |
|--------|------|----------------|----------------|
| `AGameMode` | ✅ | ❌ | ❌ |
| `AGameState` | ✅ | ✅ | ✅ |
| `APlayerController` | ✅ | ✅ | ❌ |
| `APlayerState` | ✅ | ✅ | ✅ |
| `APawn` | ✅ | ✅ | ✅ |
| `AHUD` | ❌ | ✅ | ❌ |

> 싱글플레이에서는 Authority: 1, LocalRole: 3(ROLE_Authority)으로 전부 동일하게 찍힘.  
> 멀티플레이에서는 클래스마다 Authority와 LocalRole 값이 달라짐. → **Network Role** 참고

---

## 참고 — LocalRole 값

| 값 | enum | 의미 |
|----|------|------|
| `0` | `ROLE_None` | 네트워크 무관 |
| `1` | `ROLE_SimulatedProxy` | 다른 클라이언트 입장에서 보이는 남의 액터 |
| `2` | `ROLE_AutonomousProxy` | 내 클라이언트 입장에서의 내 액터 |
| `3` | `ROLE_Authority` | 서버 (또는 싱글플레이) |
