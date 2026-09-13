# 반도체 팹 설비 캐시 & 게임 플레이어 스냅샷 - FRWLock / Reader-Writer Lock

오늘 학습한 "FRWLock / Reader-Writer Lock" 개념을 반도체 팹(Fab) 설비
상태 캐시 조회와 게임의 플레이어 스냅샷 캐시 상황에 적용해본 예제입니다.
이전 학습인 "lock granularity·lock scope·FScopeTryLock"의 연장선이며,
다음 학습("starvation·fairness·lock-free와 atomic 기초")으로 넘어가기
전에 "read가 많고 write가 적은 데이터에는 read끼리 서로 막지 않는 lock이
유리할 수 있다"는 감각을 코드와 실측으로 확인하는 것이 목적입니다.

## 상황

반도체 팹의 설비(Equipment) 상태 캐시를 여러 thread가 공유합니다.

- UI 대시보드, 로깅, 리포트 생성 등 여러 thread가 설비 상태를 **매우
  자주 읽습니다** (초당 수백~수천 번).
- 설비를 실제로 제어하는 thread는 상태를 **가끔만 갱신합니다** (설비
  자체의 물리적 변화 속도가 UI 갱신 속도보다 훨씬 느리므로).

게임에서도 구조는 동일합니다 — 여러 플레이어의 위치·체력 스냅샷을
렌더링 준비, UI, 리플레이 기록 thread가 자주 읽고, 게임플레이 thread가
틱마다 자기 자신의 값만 갱신합니다.

이런 "read 압도적 다수 + write 소수" 패턴에 **일반 mutex**를 쓰면 무슨
일이 생길까요? 그리고 **Reader-Writer Lock**을 쓰면 뭐가 달라질까요?

## 용어 미니 사전

처음 보면 헷갈리는 용어만 한 줄씩 짚고 넘어갑니다.

| 용어 | 한 줄 정의 |
|---|---|
| **Critical section** | 한 번에 하나의 thread만 실행해야 안전한 코드 구간. 보통 공유 데이터를 읽거나 쓰는 부분입니다. |
| **Mutex (상호 배제 lock)** | critical section에 "한 번에 한 thread만" 들어가게 막는 가장 기본적인 lock. UE의 `FCriticalSection`, 표준 C++의 `std::mutex`가 여기 해당합니다. |
| **Reader / Writer** | 공유 데이터를 읽기만 하는 접근자를 reader, 값을 바꾸는 접근자를 writer라고 부릅니다. |
| **Reader-Writer Lock (RW lock)** | reader끼리는 동시 접근을 허용하고, writer가 끼면 모두를 막는 lock. UE의 `FRWLock`, 표준 C++의 `std::shared_mutex`가 해당합니다. |
| **Shared lock / Exclusive lock** | RW lock을 read 모드로 잡으면 shared(공유) lock, write 모드로 잡으면 exclusive(배타) lock이라고 부릅니다. UE의 `FReadScopeLock`/`FWriteScopeLock`, 표준 C++의 `std::shared_lock`/`std::unique_lock`이 각각 대응합니다. |
| **RAII (Resource Acquisition Is Initialization)** | 객체가 생성될 때 자원을 확보하고, 소멸될 때(scope를 벗어날 때) 자동으로 해제하는 패턴. `FReadScopeLock`처럼 "scope lock"이라 불리는 것들이 이 패턴을 씁니다 — lock을 깜빡하고 안 풀어주는 실수를 원천 차단합니다. |
| **Torn read** | 값이 다 갱신되지 않은 중간 상태(예: 구조체의 절반은 새 값, 절반은 옛 값)를 읽어버리는 버그. lock 없이 read와 write가 겹치면 발생할 수 있습니다. |
| **Contention (경합)** | 여러 thread가 같은 lock을 동시에 원해서 서로 기다리게 되는 상황. |
| **Starvation (기아)** | 특정 thread(주로 writer)가 다른 thread들에게 계속 밀려서 lock을 영영 못 잡는 상황. 다음 학습 주제입니다. |

## FRWLock / Reader-Writer Lock이란

일반 mutex(`std::mutex`, UE의 `FCriticalSection`)는 "누가 들어왔든" 한
번에 한 thread만 critical section을 통과시킵니다. **reader끼리도
서로를 막습니다** — 둘 다 읽기만 하는데도 말이죠.

하지만 실무 데이터의 상당수는 "변경은 드물고 조회만 매우 잦다"는
패턴을 보입니다(설비 상태 캐시, 플레이어 스냅샷, 설정 테이블 등). 이런
경우 reader끼리는 서로 막을 이유가 없습니다 — 다들 "읽기만" 하므로
데이터가 훼손될 위험이 없기 때문입니다.

Reader-Writer Lock은 이 관찰을 반영한 동기화 도구입니다.

```
Reader + Reader -> 동시 허용 가능 (둘 다 읽기만 하므로 안전)
Reader + Writer -> 충돌 (읽는 도중 값이 바뀌면 안 됨)
Writer + Writer -> 충돌 (동시에 쓰면 데이터 훼손)
```

Unreal Engine 5.8에는 `FRWLock`과 RAII wrapper인 `FReadScopeLock`,
`FWriteScopeLock`이 있습니다. Epic API 문서는 `FReadScopeLock`이 scope가
살아있는 동안 read lock을 유지하고, `FWriteScopeLock`은 write lock을
유지한다고 설명합니다.

```cpp
FRWLock PlayerCacheLock;
TMap<int32, FPlayerSnapshot> PlayerCache;

FPlayerSnapshot GetSnapshot(int32 Id)
{
    FReadScopeLock Lock(PlayerCacheLock);
    return PlayerCache.FindRef(Id);
}

void UpdateSnapshot(int32 Id, const FPlayerSnapshot& Data)
{
    FWriteScopeLock Lock(PlayerCacheLock);
    PlayerCache.Add(Id, Data);
}
```

## 나쁜 코드 vs 좋은 코드

이 주제에는 **두 가지 서로 다른 나쁜 코드**가 있습니다. 하나는 "너무
조심스러운" 안티패턴이고, 다른 하나는 "위험할 정도로 방심한"
안티패턴입니다. 둘 다 [EquipmentCacheRWLock_UE5.h](EquipmentCacheRWLock_UE5.h)에서
전체 코드를 볼 수 있습니다.

### 나쁜 예 1 — read조차 배타적 lock으로 막기 (과도한 직렬화)

```cpp
// 나쁜 예 — read 요청조차 write와 동일한 배타적 lock을 잡는다.
// reader끼리도 서로를 기다리게 만든다.
class FEquipmentCache_Bad_MutexEveryRead
{
public:
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FScopeLock Lock(&CriticalSection);   // read인데도 배타적 lock
        return States.FindRef(EquipmentId);
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FScopeLock Lock(&CriticalSection);
        // ... 값 갱신
    }

private:
    mutable FCriticalSection CriticalSection;
    TMap<int32, FEquipmentSnapshot> States;
};
```

무엇이 문제인가: read가 압도적으로 많은 workload에서, reader들이 서로
아무 이유 없이 줄을 서서 기다립니다. 데이터를 훼손할 위험이 없는데도
"그냥 안전해 보여서" mutex 하나로 다 막아버린 것입니다.

### 나쁜 예 2 — "읽기니까 안전하겠지"라며 lock을 생략 (더 위험한 함정)

```cpp
// 나쁜 예 — read 경로에 lock이 아예 없다.
class FEquipmentCache_Bad_UnsafeRead
{
public:
    // 위험: lock이 전혀 없다. UpdateSnapshot()이 동시에 호출되면
    // undefined behavior다 (torn read, 최악의 경우 크래시).
    FEquipmentSnapshot GetSnapshot_DoNotUse(int32 EquipmentId) const
    {
        return States.FindRef(EquipmentId);
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FRWScopeLock Lock(RWLock, SLT_Write);
        // ... 값 갱신
    }

private:
    mutable FRWLock RWLock;
    TMap<int32, FEquipmentSnapshot> States;
};
```

무엇이 문제인가: **이것이 오늘 학습의 핵심 함정입니다.** "읽기만 하니까
lock 없이도 안전하다"는 생각은 틀렸습니다. read lock을 쓴다고 컨테이너가
자동으로 thread-safe해지는 게 아니라, **모든 접근 경로가 같은 동기화
규칙을 지켜야** 보호가 성립합니다. 이 코드는 write 쪽만 lock을 걸었을
뿐, read 쪽이 lock 없이 `TMap`을 들여다보고 있습니다. 다른 thread가
`UpdateSnapshot()`으로 `TMap` 내부 구조(버킷, 포인터)를 바꾸는 도중에
이 함수가 호출되면, 값이 절반만 갱신된 상태를 읽거나(torn read) 최악의
경우 크래시로 이어질 수 있습니다. **단 한 곳이라도 lock 없이
접근하면 보호 모델 전체가 깨집니다.**

### 좋은 예 — FRWLock으로 모든 접근 경로를 통일

```cpp
// 좋은 예 — read와 write 모두 FRWLock을 거치되, 모드만 다르게 잡는다.
class FEquipmentCache_Good_RWLock
{
public:
    // 여러 thread가 동시에 호출해도 서로 기다리지 않는다.
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FReadScopeLock Lock(RWLock);
        return States.FindRef(EquipmentId);
    }

    // 이 동안에는 다른 모든 reader/writer가 대기한다.
    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FWriteScopeLock Lock(RWLock);
        // ... 값 갱신
    }

private:
    mutable FRWLock RWLock;
    TMap<int32, FEquipmentSnapshot> States;
};
```

핵심 차이를 한 줄로 요약하면: **read/write 모두 반드시 lock을 거치되,
"어떤 모드로 거치는가"만 다르게 한다.** 나쁜 예 1은 두 모드를 구분하지
않아(둘 다 배타적) 불필요하게 느리고, 나쁜 예 2는 한쪽 경로가 lock
자체를 건너뛰어(read가 무방비) 위험합니다. 좋은 예는 read/write 모두
반드시 `FRWLock`을 통과시키되, read는 공유(shared) 모드로, write는
배타(exclusive) 모드로 구분해서 잡습니다.

| | 나쁜 예 1 (mutex로 read까지 배타) | 나쁜 예 2 (read에 lock 없음) | 좋은 예 (FRWLock) |
|---|---|---|---|
| 정확성 | 안전함 | **위험함 (torn read 가능)** | 안전함 |
| reader끼리 동시 실행 | 불가능 | 가능(하지만 안전하지 않음) | 가능하고 안전함 |
| read-heavy workload 성능 | 불필요하게 느림 | (정확성 문제로 비교 무의미) | 유리한 경향 |
| 실수 유발 가능성 | 낮음 (그냥 느릴 뿐) | **높음** (겉보기엔 동작하다가 드물게 깨짐) | 낮음 (구조적으로 모든 경로가 lock 통과) |

## 실험 설계

같은 캐시(설비 ID → 상태)에 대해 read:write = 200:1 비율로 접근하는
다수 thread를 돌리고, 잠금 방식만 바꿔가며 총 소요 시간을 측정합니다
([EquipmentCacheRWLock.cpp](EquipmentCacheRWLock.cpp)).

```cpp
// EquipmentCacheRWLock.cpp — ReadZone()
// 인접한 설비 여러 개를 한 번에 조회(예: 같은 Zone 대시보드 갱신)하는
// 상황을 흉내낸다. critical section을 어느 정도 늘려야 lock 방식에
// 따른 차이가 실측에 드러나기 쉬워진다.
uint64_t ReadZone(size_t EquipmentId, size_t ZoneSize) const
{
    /* lock 방식만 다름 */
    uint64_t Sum = 0;
    for (size_t i = EquipmentId; i < EquipmentId + ZoneSize; ++i)
        Sum += States[i].Version;
    return Sum;
}
```

| 실험 | 잠금 방식 | 의미 |
|---|---|---|
| A | `std::mutex` | reader도 서로 막음 (일반 mutex) |
| B | `std::shared_mutex` | reader끼리 동시 허용 (RW lock) |

두 실험 모두 "read:write 비율"과 "총 접근 횟수"는 동일합니다 — read가
압도적으로 많은 workload에서 B가 A보다 빨라야 "reader를 동시에 허용한
것"이 실제로 이득이 되는 상황임을 확인할 수 있습니다.

### 실측 결과

이 저장소에서 MSVC `/O2`, 설비 2,000개, thread당 접근 50,000회,
read:write = 200:1, zone 크기 256, 5회 반복 평균으로 측정한 결과는
다음과 같았습니다 (환경에 따라 달라질 수 있는 참고용 수치입니다).

```
설비 수: 2000, thread 수: 16, thread당 접근 횟수: 50000, read:write = 200:1, zone 크기: 256

[실험 A] std::mutex (reader도 서로 막음)
  평균 소요 시간: 188.3666 ms

[실험 B] std::shared_mutex (reader끼리 동시 허용)
  평균 소요 시간: 159.8811 ms

-> Mutex/RWLock 비율: 1.18x
```

read-heavy(200:1) workload에서 reader끼리 동시 실행을 허용한
`shared_mutex`가 일반 `mutex`보다 **약 1.18배 빨랐습니다.** 정확한
배율은 read:write 비율, critical section 길이, thread 수,
OS/컴파일러에 따라 달라지므로, 이 수치 자체보다 "read가 압도적으로
많을 때는 reader를 동시에 허용하는 쪽이 유리한 방향으로 나타난다"는
경향을 확인하는 용도로 삼아야 합니다.

**주의**: 이 벤치마크에서 read:write 비율을 20:1 정도로 낮추거나 read
critical section을 아주 짧게 만들면, RW lock 자체의 관리 비용(내부
reader 카운터 갱신, writer 대기열 처리 등) 때문에 오히려 일반 mutex와
비슷하거나 더 느려지는 결과도 관찰됩니다. **"RW lock이 항상 이긴다"가
아니라 "이 workload에서 실측해보니 이렇다"**는 태도가 중요합니다.

## 실행 방법

```bash
g++ -O2 -std=c++17 -pthread EquipmentCacheRWLock.cpp -o bench
./bench
```

Windows에서 MSVC로 빌드할 때:

```
cl /O2 /std:c++17 /EHsc EquipmentCacheRWLock.cpp
```

설비 2,000개에 대해 실험 A(std::mutex)와 실험 B(std::shared_mutex)를
각각 5회 반복해 평균 시간(ms)과 비율을 출력합니다.

### Windows에서 한글이 깨져 보인다면

이 파일은 UTF-8(BOM 포함)로 저장돼 있습니다. `AosVsSoa/PipeThermalScan.cpp`와
동일한 이유로, 화면 출력은 `printf` 대신 `PrintUtf8()`/`PrintfUtf8()`
헬퍼를 통해 나갑니다. 자세한 원리는
[EquipmentCacheRWLock.cpp](EquipmentCacheRWLock.cpp) 상단 `PrintUtf8()`
주석 및 `AosVsSoa/PipeThermalScan.cpp` 참고.

## UE5 실전 버전 — [EquipmentCacheRWLock_UE5.h](EquipmentCacheRWLock_UE5.h)

`std::mutex`/`std::shared_mutex` 대신 UE5 `FCriticalSection`/`FRWLock`을
쓴 버전입니다.

- `FEquipmentCache_Bad_MutexEveryRead` — read조차 `FCriticalSection`으로
  배타 처리하는 안티패턴 (과도한 직렬화).
- `FEquipmentCache_Bad_UnsafeRead` — read 경로에 lock을 생략한 안티패턴
  (torn read 위험). **이름 그대로 실제로 쓰면 안 되는 코드**입니다.
- `FEquipmentCache_Good_RWLock` — `FRWLock` + `FReadScopeLock` /
  `FWriteScopeLock`을 사용하는 권장 패턴.
- `FPlayerSnapshotCache` — 같은 패턴을 게임 도메인(플레이어 위치·체력
  스냅샷)에 그대로 적용한 예시. 도메인만 다를 뿐 "read가 압도적으로
  많고 write가 드문 공유 데이터"라는 본질은 동일합니다.

파일 하단의 "선택 가이드" 주석에 실무 판단 기준을 정리해두었습니다.

## 파생 개념 — 언제 FRWLock을 고려해야 하는가

오늘 학습 내용을 실무 체크리스트로 정리하면, 후보는 다음 조건을 **모두**
만족할 때입니다.

```
read 비율이 높음
  + 공유 데이터가 큼 (또는 critical section이 김)
  + mutex contention이 실측으로 확인됨
```

이 세 조건이 왜 모두 필요한지 하나씩 짚어보면:

- **read 비율이 높지 않다면** — write가 자주 섞이면 어차피 배타적
  lock을 자주 잡아야 하므로 RW lock의 이득이 사라지고, 관리 비용만
  남습니다.
- **공유 데이터가 작거나 critical section이 짧다면** — lock을 잡고
  있는 시간 자체가 워낙 짧아서, reader를 동시에 들여보내주는 이득보다
  RW lock의 내부 관리 비용(atomic 카운터 갱신 등)이 더 클 수 있습니다.
- **mutex contention이 실측되지 않았다면** — 애초에 병목이 아닌 곳을
  최적화하는 셈입니다. "이론상 유리해 보인다"가 아니라 프로파일러로
  "이 lock에서 실제로 thread들이 기다리고 있다"를 확인한 뒤 검토해야
  합니다.

### 다음 학습으로 이어지는 지점 — starvation과 fairness

`FRWLock`을 도입하면 새로운 질문이 따라옵니다: **read 요청이 끊임없이
들어오면 writer는 언제 실행되는가?**

- 만약 RW lock 구현이 "reader가 아무도 없을 때만 writer를 통과시킨다"는
  단순한 규칙을 쓴다면, read 요청이 쉴 새 없이 들어오는 상황에서는
  writer가 영원히 기다릴 수도 있습니다. 이를 **writer starvation
  (기아)** 이라고 합니다.
- 이를 막기 위해 일부 RW lock 구현은 **fairness(공정성) 정책**을
  둡니다 — 예를 들어 "writer가 이미 대기 중이면, 그 writer보다 나중에
  도착한 새 reader는 writer가 먼저 처리될 때까지 잠시 대기시킨다"는
  식입니다. 이렇게 하면 writer가 무한정 밀리는 것은 막을 수 있지만,
  이번엔 반대로 그 대기 중인 reader들의 지연이 늘어나는 trade-off가
  생깁니다.
- 다음 학습에서는 이 "누가 얼마나 기다리게 되는가"의 문제를
  starvation·fairness라는 관점에서 다루고, 더 나아가 애초에 lock 자체를
  피하는 **lock-free**·**atomic** 기법의 기초까지 이어집니다. 오늘 배운
  `FRWLock`은 "lock을 더 똑똑하게 나눈 것"이라면, lock-free는 "애초에
  lock을 걸지 않고도 안전하게 공유하는 것"을 목표로 하는, 한 단계 더
  나아간 접근입니다.

## 결론 요약

- `read가 압도적으로 많고 write가 드묾` → `FRWLock`(`FReadScopeLock`
  /`FWriteScopeLock`) 검토 대상
- `read/write 비율이 비슷하거나 critical section이 매우 짧음` →
  `FCriticalSection`(일반 mutex)이 더 단순하고 유리할 수 있음
- **read 경로도 반드시 lock을 거쳐야 한다** — "읽기니까 안전하다"는
  가정은 틀렸다. 모든 접근 경로가 같은 lock 규칙을 지켜야 보호가 성립함
- `FRWLock`이 항상 더 빠른 것은 아니다 — 목표 플랫폼에서 Unreal
  Insights와 벤치마크를 통해 항상 실측 확인
- 다음 단계: writer starvation·fairness, 그리고 lock-free/atomic 기초

## 참고 자료

- Epic UE 5.8 FRWLock API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/HAL/FRWLock
- Epic UE 5.8 FReadScopeLock / FWriteScopeLock: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Misc/FReadScopeLock
- cppreference std::shared_mutex: https://en.cppreference.com/w/cpp/thread/shared_mutex

## 발표자료

- [PRESENTATION.md](PRESENTATION.md) — 문제 상황부터 실측 비교, UE5
  적용까지 발표용으로 정리한 문서.
