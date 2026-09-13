# 발표자료 — read가 많으면 lock도 다르게 잡아야 한다

오늘 학습한 "FRWLock / Reader-Writer Lock" 개념을, 반도체 팹 설비 상태
캐시와 게임 플레이어 스냅샷 캐시 상황으로 재구성하고 실측 수치까지 뽑아
정리했다. 발표 흐름: **개념 → 실무 사용처 → 문제 상황(나쁜 코드 두
종류) → 실험 설계 → 실측 비교 → UE5 실전 적용 → 결론**.

---

## 0. 개념 — Reader-Writer Lock이란

### 비유로 먼저

도서관 열람실을 생각해보자.

- **책을 읽기만 하는 사람(reader)들**은 같은 책이든 다른 책이든 몇
  명이 동시에 들어와 있어도 서로 방해가 되지 않는다.
- 하지만 **사서가 책 내용을 고쳐 쓰는 동안(writer)**에는, 그 누구도
  그 책을 읽거나 다른 사서가 동시에 고쳐 쓰게 둘 수 없다 — 고치는
  도중의 내용을 읽으면 이상한 내용을 보게 되고, 두 사서가 동시에
  고치면 내용이 뒤섞인다.

일반 mutex는 "열람실에 한 번에 한 명만 들어올 수 있다"는 규칙과 같다 —
읽기만 하려는 사람도 줄을 서야 한다. Reader-Writer Lock은 "읽기만 하는
사람은 여러 명이 동시에 들어와도 되지만, 고쳐 쓰는 사람이 들어오면
그 순간만큼은 혼자 들어와야 한다"는, 더 현실적인 규칙이다.

```
Reader + Reader -> 동시 허용 가능 (둘 다 읽기만 하므로 안전)
Reader + Writer -> 충돌 (읽는 도중 값이 바뀌면 안 됨)
Writer + Writer -> 충돌 (동시에 쓰면 데이터 훼손)
```

Unreal Engine 5.8은 `FRWLock`과 RAII wrapper인 `FReadScopeLock`,
`FWriteScopeLock`을 제공한다. `FReadScopeLock`은 scope가 살아있는 동안
read lock을 유지하고, `FWriteScopeLock`은 write lock을 유지한다(Epic
공식 API 설명).

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

### 두 가지 함정 — 이 발표에서 가장 강조하고 싶은 부분

1. **"read lock을 쓴다고 컨테이너가 자동으로 thread-safe해지는 것은
   아니다."** 모든 접근자가 같은 동기화 규칙을 지켜야 한다. 한 코드
   경로가 lock 없이 write(혹은 read)하면 보호 모델이 깨진다.
2. **"RW lock이 항상 mutex보다 빠른 것도 아니다."** write가 많거나
   critical section이 짧으면 관리 비용 때문에 이점이 작을 수 있고,
   구현에 따라 writer 또는 reader가 오래 기다리는 starvation 문제도
   고려해야 한다.

### 그래서 언제 뭘 쓰나 — 한눈에 요약

| 상황 | 권장 |
|---|---|
| read 비율이 압도적으로 높음 + 공유 데이터가 큼 + mutex contention 실측됨 | `FRWLock` 검토 |
| read/write 비율이 비슷함 (또는 write가 잦음) | 일반 `FCriticalSection`이 더 단순하고 유리할 수 있음 |
| critical section이 아주 짧음 | `FRWLock`의 관리 비용이 이득을 상쇄할 수 있으므로 실측 필요 |

다만 이 표는 "이론적으로 기대할 수 있는 경향"이다. 4장의 실측 결과가
그 크기를 구체적으로 보여준다.

---

## 0-1. 실무에서 이 개념이 자주 등장하는 상황

| 상황 | 왜 문제가 되는가 |
|---|---|
| **게임의 공유 상태 캐시** (플레이어 위치·체력 스냅샷) | 렌더링 준비, UI, 리플레이 기록 등 여러 시스템이 매 프레임 다른 플레이어의 상태를 읽는다. 반면 그 플레이어 본인의 게임플레이 thread는 훨씬 드물게 갱신한다. 모든 read를 배타적 mutex로 막으면 프레임마다 불필요한 대기가 쌓인다. |
| **설정/자산 테이블 캐시** | 게임 실행 중 설정값이나 로드된 자산 정보를 여러 시스템이 자주 참조하지만, 값 자체는 핫리로드나 관리자 명령이 있을 때만 드물게 바뀐다. |
| **반도체 Fab 대시보드** | UI·로깅·리포트 thread가 설비 상태를 초당 수백 번 읽고, 설비 제어 thread는 물리적 변화 속도에 맞춰 훨씬 느리게 갱신한다. |
| **서버의 read-heavy 캐시** (세션 정보, 리더보드 등) | 다수의 요청 처리 thread가 캐시를 조회하고, 갱신은 별도 배치/주기적 작업에서만 발생하는 구조에서 RW lock이 자주 채택된다. |

공통점: **"이 데이터를 얼마나 자주 읽는가"와 "얼마나 자주 바꾸는가"의
비율이 크게 벌어질 때, 그 비대칭을 lock 설계에 반영하면 이득을 볼 수
있다.**

---

## 1. 문제 상황 — 나쁜 코드는 사실 두 종류다

read-heavy 공유 데이터를 다룰 때 초보자가 빠지기 쉬운 실수는 정반대
방향으로 두 가지다.

### 나쁜 코드 1 — 너무 조심스러움 (read까지 배타적으로 막음)

```cpp
class FEquipmentCache_Bad_MutexEveryRead
{
public:
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FScopeLock Lock(&CriticalSection);   // read인데도 배타적 lock
        return States.FindRef(EquipmentId);
    }
    // ...
private:
    mutable FCriticalSection CriticalSection;
    TMap<int32, FEquipmentSnapshot> States;
};
```

reader끼리도 서로 줄을 세운다. 데이터가 훼손될 위험이 없는데도 "안전해
보인다"는 이유만으로 불필요한 직렬화를 만든다.

### 나쁜 코드 2 — 너무 방심함 (read에 lock을 아예 생략)

```cpp
class FEquipmentCache_Bad_UnsafeRead
{
public:
    // 위험: lock이 전혀 없다.
    FEquipmentSnapshot GetSnapshot_DoNotUse(int32 EquipmentId) const
    {
        return States.FindRef(EquipmentId);   // torn read 위험
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FRWScopeLock Lock(RWLock, SLT_Write);
        // ...
    }
private:
    mutable FRWLock RWLock;
    TMap<int32, FEquipmentSnapshot> States;
};
```

**이것이 오늘 발표에서 가장 강조하고 싶은 함정이다.** "읽기만 하니까
안전하다"는 생각은 틀렸다. write 쪽만 lock을 걸고 read 쪽을 그냥
두면, 다른 thread가 `TMap` 내부 구조를 바꾸는 도중에 이 함수가
호출될 수 있고, 그 결과 torn read나 크래시로 이어질 수 있다.

나쁜 코드 1은 "느리지만 안전"하고, 나쁜 코드 2는 "빠르지만 위험"하다.
**정답은 그 사이 어딘가가 아니라, read/write 모두 반드시 lock을 거치되
모드만 다르게 잡는 것**이다.

```cpp
class FEquipmentCache_Good_RWLock
{
public:
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FReadScopeLock Lock(RWLock);      // 공유 모드
        return States.FindRef(EquipmentId);
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FWriteScopeLock Lock(RWLock);     // 배타 모드
        // ...
    }
private:
    mutable FRWLock RWLock;
    TMap<int32, FEquipmentSnapshot> States;
};
```

---

## 2. 실험 설계

같은 캐시(설비 ID → 상태)에 대해 read:write = 200:1 비율로 접근하는
다수 thread를 돌리고, 잠금 방식만 바꿔가며 총 소요 시간을 측정한다.

```cpp
// EquipmentCacheRWLock.cpp — ReadZone()
// 인접한 설비 여러 개를 한 번에 조회(같은 Zone 대시보드 갱신 상황)한다.
// critical section을 어느 정도 늘려야 lock 방식에 따른 차이가
// 실측에 드러나기 쉬워진다.
uint64_t ReadZone(size_t EquipmentId, size_t ZoneSize) const
{
    /* lock 방식만 다름 (std::mutex vs std::shared_mutex) */
    uint64_t Sum = 0;
    for (size_t i = EquipmentId; i < EquipmentId + ZoneSize; ++i)
        Sum += States[i].Version;
    return Sum;
}
```

| 실험 | 잠금 방식 | 의미 |
|---|---|---|
| A | `std::mutex` | reader도 서로 막음 |
| B | `std::shared_mutex` | reader끼리 동시 허용 |

두 실험 모두 **read:write 비율과 총 접근 횟수는 동일**하다 — 잠금
방식만 다르다. read-heavy 상황에서 B가 A보다 빠르다면, 그것이 곧
"reader를 동시에 허용한 것"의 실제 이득이다.

---

## 3. 실측 결과

MSVC `/O2`, 설비 2,000개, thread 16개, thread당 접근 50,000회,
read:write = 200:1, zone 크기 256, 5회 반복 평균으로 측정했다.

```
설비 수: 2000, thread 수: 16, thread당 접근 횟수: 50000, read:write = 200:1, zone 크기: 256

[실험 A] std::mutex (reader도 서로 막음)
  평균 소요 시간: 188.3666 ms

[실험 B] std::shared_mutex (reader끼리 동시 허용)
  평균 소요 시간: 159.8811 ms

-> Mutex/RWLock 비율: 1.18x
```

> 재현: `EquipmentCacheRWLock.cpp`를 컴파일해 실행하면 동일한 방식으로
> 직접 확인할 수 있다.
> (`g++ -O2 -std=c++17 -pthread EquipmentCacheRWLock.cpp -o bench`)

### 왜 이렇게 나왔는가

- read:write = 200:1로 read가 압도적으로 많은 workload에서는, reader
  16개가 서로 기다릴 필요 없이 동시에 캐시를 읽을 수 있는
  `shared_mutex`가 유리했다.
- 반대로 이 저장소에서 read:write 비율을 20:1 정도로 낮추거나 read
  critical section을 짧게(zone 크기를 줄여) 만들면, 두 방식의 차이가
  거의 사라지거나 오히려 `shared_mutex`가 근소하게 느려지는 경우도
  관찰됐다 — RW lock 내부의 reader 카운팅·writer 대기열 관리 비용이
  이득을 상쇄하기 때문으로 보인다.

**이 대비가 발표의 핵심 포인트다.** "read가 많으면 무조건 RW lock이
이득"이 아니라, *read 비율이 충분히 높고 critical section이 충분히
길 때만* 그 이득이 실측으로 드러난다 — 오늘 학습한 "read 비율이 높음 +
공유 데이터가 큼 + mutex contention이 측정됨"이라는 세 조건이 왜
동시에 필요한지를 이 실험이 그대로 보여준다.

---

## 4. C++ → UE5, 두 버전의 관계

| 파일 | 역할 |
|---|---|
| [EquipmentCacheRWLock.cpp](EquipmentCacheRWLock.cpp) | **벤치마크** — 표준 C++(`std::mutex` / `std::shared_mutex`)로 두 방식을 구현하고 실측 시간을 직접 측정 |
| [EquipmentCacheRWLock_UE5.h](EquipmentCacheRWLock_UE5.h) | **실전 적용** — 나쁜 코드 두 종류(`FEquipmentCache_Bad_MutexEveryRead`, `FEquipmentCache_Bad_UnsafeRead`)와 권장 패턴(`FEquipmentCache_Good_RWLock`)을 대비, 게임 도메인 예시(`FPlayerSnapshotCache`)까지 포함 |

```cpp
// EquipmentCacheRWLock_UE5.h — 위험한 안티패턴: read 경로에 lock이 없음
FEquipmentSnapshot GetSnapshot_DoNotUse(int32 EquipmentId) const
{
    return States.FindRef(EquipmentId); // 다른 thread의 write와 경합하면 torn read
}
```

```cpp
// EquipmentCacheRWLock_UE5.h — 권장: read/write 모두 FRWLock을 통과
FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
{
    FReadScopeLock Lock(RWLock);
    return States.FindRef(EquipmentId);
}
```

**핵심 메시지**: 같은 패턴(read-heavy 공유 캐시)이 반도체 Fab의 설비
상태든, 게임의 플레이어 스냅샷이든 동일하게 적용된다. 도메인이 달라도
"read가 압도적으로 많고 write가 드물다"는 조건만 성립하면 `FRWLock`
도입을 검토할 근거가 된다.

### 다음 학습(starvation·fairness)과의 관계

`FRWLock`을 도입하면 새로운 질문이 생긴다 — read 요청이 끊임없이
들어오면 writer는 언제 실행되는가? 일부 구현은 "reader가 아무도 없을
때만 writer 통과"라는 단순 규칙을 쓰는데, 이 경우 read가 쉴 새 없이
들어오면 writer가 무한정 밀리는 **starvation**이 발생할 수 있다. 이를
막기 위한 **fairness 정책**(예: writer가 대기 중이면 새 reader를 잠시
막기)은 다음 학습에서 다룬다. 나아가 "애초에 lock을 걸지 않고 안전하게
공유하는" **lock-free/atomic** 기법도 이 흐름의 연장선에 있다.

---

## 5. 발표 요약 한 줄

> **"read가 많으니까 lock을 다 없애자"는 위험하고(torn read), "일단
> 안전하게 다 막자"는 비효율적이다(불필요한 직렬화) — 이번 벤치마크에서는
> read:write 200:1인 workload에서 FRWLock(std::shared_mutex)이 일반
> mutex보다 약 1.18배 빨랐고, 그 이유는 reader끼리 서로 기다릴 필요가
> 없어졌기 때문이다. 다만 이 이득은 read 비율이 충분히 높고 critical
> section이 충분히 길 때만 실측으로 확인되므로, "RW lock이 항상
> 이긴다"가 아니라 목표 workload에서 직접 측정하고 판단해야 한다.**

---

## 부록 — 참고 자료

- Epic UE 5.8 FRWLock API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/HAL/FRWLock
- Epic UE 5.8 FReadScopeLock / FWriteScopeLock: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Misc/FReadScopeLock
- cppreference std::shared_mutex: https://en.cppreference.com/w/cpp/thread/shared_mutex

## 부록 — 면접 30초 요약

> "일반 mutex는 reader도 한 번에 한 thread만 통과시키는데, 데이터
> 변경이 드물고 읽기만 매우 많다면 이는 불필요한 직렬화입니다.
> Reader-Writer Lock은 reader끼리는 동시 허용하고 writer가 끼면 모두
> 막는 방식으로 이를 개선합니다. 실제로 read:write 200:1인 캐시 조회를
> 벤치마크했더니 FRWLock 방식이 일반 mutex보다 약 1.18배 빨랐습니다.
> 다만 두 가지를 꼭 지켰습니다 — 첫째, read 경로도 반드시 lock을
> 거쳐야 한다는 것(그렇지 않으면 torn read 위험), 둘째, write 비율이
> 높거나 critical section이 짧으면 RW lock이 오히려 손해일 수 있다는
> 것입니다. 그래서 read 비율이 높고 공유 데이터가 크며 mutex
> contention이 실측된 경우에만 FRWLock을 도입하고, starvation·fairness는
> 다음 단계로 검토하겠습니다."
