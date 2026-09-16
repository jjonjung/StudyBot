# 2026-09-16 DEV TECH 학습 노트 — memory_order_relaxed vs memory_order_seq_cst

커리큘럼 위치: mutex → RW lock → atomic → release/acquire → **relaxed vs seq_cst** (다음: atomic RMW와 ABA 문제)

---

## 1. 개념 요약 (쉬운 설명)

`std::atomic`의 연산은 memory order를 따로 지정하지 않으면 기본값인 `memory_order_seq_cst`(sequentially consistent)를 사용합니다.
`seq_cst`는 지난 노트의 release-acquire가 주는 보장(그 원자 변수를 매개로 한 일반 메모리의 가시성)에 더해,
**프로그램 전체의 모든 `seq_cst` 연산들 사이에 하나의 전역적인 total order**까지 보장합니다. 즉 "이 순서로 일어난 걸 모든 스레드가 동일하게 본다"는 가장 강한 약속입니다.

반대로 `memory_order_relaxed`는 딱 하나만 보장합니다.

- **보장하는 것**: 그 atomic 변수 자체의 atomicity(찢어진 값이 보이지 않음)와 modification order(그 변수 하나에 대한 쓰기들의 순서는 모든 스레드가 동일하게 관찰함).
- **보장하지 않는 것**: 그 변수를 기준으로 한 다른 메모리 접근의 동기화. release-acquire처럼 "이 값을 봤으면 그 전에 쓴 다른 데이터도 안전하다"는 관계가 전혀 성립하지 않습니다.

그래서 relaxed는 **다른 데이터의 신호로 쓰이지 않는, 그 값 자체만 중요한 카운터/통계**에 적합합니다.

```cpp
std::atomic<uint64_t> TotalHits{0};
void RecordHit() { TotalHits.fetch_add(1, std::memory_order_relaxed); }
```

필요한 건 "여러 thread가 증가시켜도 값이 유실되지 않는다"이지, "이 counter를 본 thread가 다른 메모리 변경까지 특정 순서로 본다"가 아니기 때문입니다.

중요한 건 "seq_cst는 느리고 relaxed는 빠르다"는 단순 비교가 아니라는 점입니다. 실제 추가 비용은 CPU 아키텍처와 연산 종류에 따라 다르고,
correctness를 잃으면 성능 이득은 의미가 없습니다. **먼저 가장 단순하고 강한 ordering(seq_cst)으로 정확성을 확보하고, 프로파일링에서
atomic ordering이 실제 병목으로 확인될 때만 relaxed로 완화를 검토**하는 순서가 안전합니다.

**면접 30초 요약**: "Relaxed는 atomic 객체 자체의 원자성과 modification order만 필요할 때 적합하고 다른 메모리의 synchronization은 제공하지 않습니다.
Seq_cst는 더 강한 전역 순서 모델을 제공해 reasoning이 쉽습니다. 먼저 correctness를 확보하고 실제 병목이 확인될 때만 ordering을 낮추겠습니다."

---

## 2. Bad Code — 문제가 있는 코드 (relaxed로 "동시 조건 감지"를 잘못 구현)

협동 액션 게임에서, 서로 다른 두 시스템 스레드가 보스(대왕문어) 연출 조건을 각각 독립적으로 갱신하고,
세 번째 스레드(이벤트 매니저)가 "두 조건이 동시에 true가 된 순간"을 감지해 페이즈 전환 컷씬을 트리거하는 상황입니다.

```cpp
#include <atomic>

std::atomic<bool> bArenaHazardActive{false};   // [환경 스레드] 독가스 웅덩이 활성화 여부
std::atomic<bool> bBossEnraged{false};         // [AI 스레드] 보스 분노 상태 여부

// [환경 스레드]
void EnvironmentThread_Bad()
{
    SpawnHazardVolume();
    bArenaHazardActive.store(true, std::memory_order_relaxed);
}

// [AI 스레드]
void BossAiThread_Bad()
{
    ApplyEnrageBuff();
    bBossEnraged.store(true, std::memory_order_relaxed);
}

// [이벤트 매니저 스레드] 두 조건이 "동시에" true인 순간 컷씬 트리거
void EventManagerThread_Bad()
{
    bool bHazard  = bArenaHazardActive.load(std::memory_order_relaxed);
    bool bEnraged = bBossEnraged.load(std::memory_order_relaxed);

    // 문제: relaxed는 서로 다른 두 atomic 변수 사이의 "전역 순서"를 보장하지 않는다.
    // 컴파일러/CPU 재배치로 인해 실제로는 둘 다 true가 된 프레임이 있었는데도
    // 이 스레드에서는 (false, true) 또는 (true, false)처럼 어긋난 조합으로 관찰되어
    // 컷씬이 아예 트리거되지 않거나, 잘못된 타이밍에 트리거될 수 있다.
    if (bHazard && bEnraged)
    {
        TriggerPhaseTransitionCutscene();
    }
}
```

**왜 문제인가:** `bArenaHazardActive`와 `bBossEnraged`는 서로 다른 atomic 변수입니다. release-acquire조차도 "하나의 변수를 매개로 한"
동기화만 보장하기 때문에, 독립된 두 변수를 각각 다른 스레드가 쓰고 세 번째 스레드가 "둘 다 관찰되는 순서"에 의존한다면 release-acquire로도
부족합니다. relaxed는 말할 것도 없습니다. 이런 패턴에서는 **모든 관련 연산이 하나의 전역 total order 안에 있어야** 하며, 그걸 보장하는 건 `seq_cst`뿐입니다.

---

## 3. Refactored Code — seq_cst로 고친 코드

```cpp
#include <atomic>

std::atomic<bool> bArenaHazardActive{false};
std::atomic<bool> bBossEnraged{false};

// [환경 스레드]
void EnvironmentThread_Fixed()
{
    SpawnHazardVolume();
    bArenaHazardActive.store(true, std::memory_order_seq_cst); // 기본값이지만 의도를 명시
}

// [AI 스레드]
void BossAiThread_Fixed()
{
    ApplyEnrageBuff();
    bBossEnraged.store(true, std::memory_order_seq_cst);
}

// [이벤트 매니저 스레드]
void EventManagerThread_Fixed()
{
    bool bHazard  = bArenaHazardActive.load(std::memory_order_seq_cst);
    bool bEnraged = bBossEnraged.load(std::memory_order_seq_cst);

    // seq_cst는 프로그램의 모든 seq_cst 연산 사이에 하나의 전역 total order를 강제하므로,
    // 두 store가 실제로 일어난 순서와 모순되는 (false, true) / (true, false) 관찰이 나타나지 않는다.
    if (bHazard && bEnraged)
    {
        TriggerPhaseTransitionCutscene();
    }
}
```

바뀐 건 memory order 태그뿐이지만(사실 `seq_cst`는 `std::atomic`의 기본값이라 태그를 생략해도 동일하게 동작합니다 —
여기서는 의도를 코드에 남기기 위해 명시했습니다), 이걸로 "서로 다른 두 atomic 변수를 여러 스레드가 관찰할 때도 순서가 뒤틀리지
않는다"는 보장이 생깁니다. 반대로 `TotalHits`처럼 **단일 변수의 값 자체만 중요하고 다른 메모리와 무관한 카운터**는 여전히
`relaxed`가 더 적합합니다 — 모든 곳에 seq_cst를 쓰는 것도 "생각 없이 강하게만 가는" 또 다른 형태의 안일함입니다.

---

## 4. 적용 시나리오

### 게임 엔진 / AQUALOS 프로젝트 (협동 액션 게임)

- **relaxed가 맞는 경우**: 여러 플레이어가 대왕문어에게 가한 누적 데미지·히트 카운트, 처치한 촉수 개수 같은 **텔레메트리/통계 카운터**.
  다른 스레드가 "이 값을 봤으니 다른 데이터도 준비됐다"고 판단할 필요가 없는, 숫자 자체만 맞으면 되는 값이라면 relaxed로 충분합니다.
- **seq_cst가 필요한 경우**: 위 예시처럼 여러 스레드가 각각 설정하는 **독립적인 상태 플래그 여러 개를 조합해서 판단**하는 로직
  (페이즈 전환, 협동 콤보 성립 판정, 멀티플레이어 동시 입력 윈도우 체크 등). 이런 곳에 relaxed나 심지어 release-acquire를 써도
  간헐적으로 조건이 씹히는, 재현하기 매우 어려운 버그가 됩니다.

### CAD / 반도체 팹 자동화 (기존 FabEquipmentRegistry 계열)

- **relaxed가 맞는 경우**: 설비(equipment) 컨트롤러가 처리한 누적 lot 수, 처리 시간 합계 같은 모니터링용 통계 카운터.
- **seq_cst가 필요한 경우**: 서로 다른 설비 컨트롤러 스레드가 각각 `bTemperatureAlarm`, `bPressureAlarm` 같은 알람 플래그를 설정하고,
  안전 감시(watchdog) 스레드가 "두 알람이 동시에 발생했는지"를 감지해 비상정지(E-stop)를 트리거해야 하는 경우. 여기서 false negative
  (실제로는 동시에 발생했는데 감지를 못 하는 것)는 안전 사고로 이어질 수 있으므로, 성능보다 correctness가 훨씬 중요합니다.

---

## 5. 리팩토링 체크리스트 — 기존 코드에 적용하는 법

1. **atomic 변수를 grep으로 전부 나열**하고, 각각을 두 그룹으로 분류합니다.
   - A그룹: "그 값 자체"만 의미 있음 (카운터, 통계, 텔레메트리) → `relaxed` 후보.
   - B그룹: "이 값을 봤다"가 다른 메모리/다른 atomic 변수의 상태에 대한 신호로 쓰임 → `release/acquire` 또는 `seq_cst` 후보.
2. B그룹 안에서 다시 나눕니다.
   - 하나의 producer-consumer 쌍, 하나의 atomic 변수를 매개로 한 "1회성 발행" → `release/acquire` (이전 노트 참고).
   - **서로 다른 atomic 변수 2개 이상을 여러 스레드가 조합해서 판단** → `seq_cst` (오늘 노트).
3. 지금 당장은 확신이 안 서는 변수는 일단 `seq_cst`(기본값)로 두고 주석으로 "왜 강한 ordering을 쓰는지" 대신
   "아직 relaxed로 낮춰도 되는지 검증 전"이라고 남겨둡니다. 프로파일링에서 해당 atomic 연산이 hot path로 확인되면 그때
   A/B 분류를 다시 검토해 좁힙니다.
4. ordering을 낮출 때마다 **반드시 멀티스레드 스트레스 테스트(반복 실행, ThreadSanitizer 등)를 함께** 추가합니다. ordering 버그는
   대부분 낮은 확률로만 재현되기 때문에, "로컬에서 안 터졌다"는 검증 근거가 되지 못합니다.

---

*다음 학습: atomic RMW(read-modify-write)와 ABA 문제.*
