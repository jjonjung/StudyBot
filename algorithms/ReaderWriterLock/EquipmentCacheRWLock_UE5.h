// EquipmentCacheRWLock_UE5.h
//
// EquipmentCacheRWLock.cpp(순수 C++, std::shared_mutex)와 동일한 "설비
// 상태 캐시 조회" 작업을, UE5 실전 코드로 옮긴 버전. UE5 프로젝트(예:
// UnrealStudyBot)에서 반도체 팹 대시보드나 멀티플레이어 게임의 공유 상태
// 캐시를 만든다고 가정한다.
//
// 핵심 메시지: 오늘 학습한 "read가 압도적으로 많고 write가 드문 공유
// 데이터"에는 FCriticalSection(일반 mutex) 대신 FRWLock을 검토하되,
// 두 가지 함정(① lock 규칙을 모든 접근 경로가 지켜야 함, ② RW lock이
// 항상 더 빠른 것은 아님)을 함께 코드로 보여준다.
//
//   - FEquipmentCache_Bad_MutexEveryRead : read조차 FCriticalSection으로
//     배타 처리하는 안티패턴. reader끼리도 서로 기다린다.
//   - FEquipmentCache_Bad_UnsafeRead     : "읽기니까 안전하겠지"라며 lock을
//     생략한 안티패턴. write와 경합하면 torn read(값이 반쯤 갱신된 상태를
//     읽는 것)가 발생할 수 있다.
//   - FEquipmentCache_Good_RWLock        : FRWLock + FReadScopeLock /
//     FWriteScopeLock을 사용하는 권장 패턴.
//
// 두 가지 도메인 예시를 함께 담았다.
//   1) 반도체 Fab 대시보드: 여러 UI/로깅 thread가 설비 상태를 자주 읽고,
//      설비 제어 thread가 가끔 갱신한다.
//   2) 게임: 여러 시스템(렌더링 준비, UI, 리플레이 기록)이 플레이어
//      스냅샷을 자주 읽고, 게임플레이 thread가 가끔 갱신한다.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeRWLock.h"
#include "HAL/CriticalSection.h"
#include "Containers/Map.h"

// ---------------------------------------------------------------------
// 공유 데이터: 설비 상태 스냅샷. 반도체 Fab 대시보드와 게임의 "플레이어
// 스냅샷 캐시"를 하나의 구조로 대표한다 - 도메인만 다를 뿐 "여러 필드를
// 가진 값을 read-heavy하게 공유한다"는 본질은 같다.
// ---------------------------------------------------------------------
struct FEquipmentSnapshot
{
    float Temperature = 20.0f;
    float Pressure = 1.0f;
    uint64 Version = 0;
};

// =======================================================================
// 안티패턴 1 — read조차 배타적 lock(FCriticalSection)으로 처리
// =======================================================================
//
// 가장 먼저 떠오르는 "일단 안전하게" 버전. 정확성은 문제없지만, reader가
// 아무리 늘어나도 한 번에 하나씩만 통과하므로 read-heavy 상황에서
// 불필요한 직렬화가 생긴다. UI가 여러 곳에서 초당 수백 번씩 이 캐시를
// 읽는 대시보드 상황이라면 이 직렬화 자체가 병목이 될 수 있다.
class FEquipmentCache_Bad_MutexEveryRead
{
public:
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FScopeLock Lock(&CriticalSection);
        if (const FEquipmentSnapshot* Found = States.Find(EquipmentId))
        {
            return *Found;
        }
        return FEquipmentSnapshot{};
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FScopeLock Lock(&CriticalSection);
        FEquipmentSnapshot& State = States.FindOrAdd(EquipmentId);
        State.Temperature = NewTemperature;
        State.Pressure = NewPressure;
        ++State.Version;
    }

private:
    mutable FCriticalSection CriticalSection;
    TMap<int32, FEquipmentSnapshot> States;
};

// =======================================================================
// 안티패턴 2 — "읽기만 하니까 안전하겠지"라며 lock을 생략
// =======================================================================
//
// 겉보기엔 그럴듯하다 - "나는 값을 바꾸지 않고 읽기만 하니까 lock이
// 필요 없다"고 생각하기 쉽다. 하지만 이건 **오늘 학습의 핵심 함정**이다:
// read lock을 잡든 안 잡든, 컨테이너 자체가 thread-safe해지는 게 아니다.
// 다른 thread가 UpdateSnapshot()으로 TMap 내부 구조(버킷, 포인터)를
// 바꾸는 도중에 이 GetSnapshot()이 그 TMap을 순회/조회하면, 값이 절반만
// 갱신된 상태를 읽거나(torn read) 최악의 경우 크래시로 이어질 수 있다.
// "read니까 괜찮다"는 보장은 모든 접근자가 동일한 lock 규칙을 지킬
// 때만 성립한다 - 단 한 곳이라도 이렇게 lock 없이 접근하면 보호 모델
// 전체가 깨진다.
class FEquipmentCache_Bad_UnsafeRead
{
public:
    // 위험: lock이 전혀 없다. UpdateSnapshot()이 동시에 호출되면
    // undefined behavior다.
    FEquipmentSnapshot GetSnapshot_DoNotUse(int32 EquipmentId) const
    {
        if (const FEquipmentSnapshot* Found = States.Find(EquipmentId))
        {
            return *Found; // 다른 thread가 States를 갱신 중이면 torn read 위험
        }
        return FEquipmentSnapshot{};
    }

    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FRWScopeLock Lock(RWLock, SLT_Write);
        FEquipmentSnapshot& State = States.FindOrAdd(EquipmentId);
        State.Temperature = NewTemperature;
        State.Pressure = NewPressure;
        ++State.Version;
    }

private:
    mutable FRWLock RWLock; // Write 쪽만 lock을 걸어봤자 read가 안 걸면 의미 없다
    TMap<int32, FEquipmentSnapshot> States;
};

// =======================================================================
// 권장 패턴 — FRWLock + FReadScopeLock / FWriteScopeLock
// =======================================================================
//
//   Reader + Reader -> 동시 허용 (둘 다 읽기만 하므로 안전)
//   Reader + Writer -> 충돌 (읽는 도중 값이 바뀌면 안 됨)
//   Writer + Writer -> 충돌 (동시에 쓰면 데이터 훼손)
//
// FReadScopeLock은 scope가 살아있는 동안 read lock을, FWriteScopeLock은
// write lock을 유지하는 RAII wrapper다(Epic 공식 API 문서 설명). 이
// 클래스의 모든 접근 경로(읽기든 쓰기든)가 예외 없이 이 lock을 통과하기
// 때문에, 안티패턴 2처럼 "한 경로만 lock을 빼먹는" 실수가 구조적으로
// 방지된다 - private 멤버에 직접 접근할 방법이 없고 오직 이 두 함수를
// 통해서만 States에 닿을 수 있기 때문이다.
class FEquipmentCache_Good_RWLock
{
public:
    // 여러 UI/로깅/렌더링 thread가 동시에 호출해도 서로 기다리지 않는다.
    FEquipmentSnapshot GetSnapshot(int32 EquipmentId) const
    {
        FReadScopeLock Lock(RWLock);
        if (const FEquipmentSnapshot* Found = States.Find(EquipmentId))
        {
            return *Found;
        }
        return FEquipmentSnapshot{};
    }

    // 설비 제어(또는 게임플레이) thread가 가끔 호출한다. 이 동안에는
    // 다른 모든 reader/writer가 대기한다.
    void UpdateSnapshot(int32 EquipmentId, float NewTemperature, float NewPressure)
    {
        FWriteScopeLock Lock(RWLock);
        FEquipmentSnapshot& State = States.FindOrAdd(EquipmentId);
        State.Temperature = NewTemperature;
        State.Pressure = NewPressure;
        ++State.Version;
    }

private:
    mutable FRWLock RWLock;
    TMap<int32, FEquipmentSnapshot> States;
};

// =======================================================================
// 게임 도메인 예시 — 플레이어 스냅샷 캐시
// =======================================================================
//
// 반도체 Fab의 "설비 상태"를 게임의 "플레이어 스냅샷"으로 바꿔도 구조는
// 완전히 동일하다. 이것이 이 개념이 "read가 압도적으로 많고 write가
// 드문 공유 데이터"라는 패턴 자체에 적용되는 범용 도구라는 뜻이다.
//
//   - Read: 렌더링 준비(다른 플레이어 위치 보간), UI(스코어보드),
//     리플레이 기록 thread가 초당 수십~수백 번 조회
//   - Write: 게임플레이 thread가 틱마다 자기 자신의 상태만 갱신
struct FPlayerSnapshot
{
    FVector Location = FVector::ZeroVector;
    float Health = 100.0f;
    uint64 Version = 0;
};

class FPlayerSnapshotCache
{
public:
    FPlayerSnapshot GetSnapshot(int32 PlayerId) const
    {
        FReadScopeLock Lock(RWLock);
        if (const FPlayerSnapshot* Found = Snapshots.Find(PlayerId))
        {
            return *Found;
        }
        return FPlayerSnapshot{};
    }

    void UpdateSnapshot(int32 PlayerId, const FVector& NewLocation, float NewHealth)
    {
        FWriteScopeLock Lock(RWLock);
        FPlayerSnapshot& Snapshot = Snapshots.FindOrAdd(PlayerId);
        Snapshot.Location = NewLocation;
        Snapshot.Health = NewHealth;
        ++Snapshot.Version;
    }

private:
    mutable FRWLock RWLock;
    TMap<int32, FPlayerSnapshot> Snapshots;
};

// ---------------------------------------------------------------------
// 선택 가이드 (오늘 학습 결론을 이 도메인에 맞게 정리)
// ---------------------------------------------------------------------
//
// - "read가 압도적으로 많고 write는 드물다 + 공유 데이터가 크다(또는
//   critical section이 길다) + mutex contention이 실측으로 확인됐다"
//   조건을 모두 만족할 때 FRWLock을 검토한다. FEquipmentCache_Bad_
//   MutexEveryRead처럼 항상 배타적으로 막는 것보다 유리할 가능성이 있다.
//
// - 절대로 FEquipmentCache_Bad_UnsafeRead처럼 "읽기니까 lock 없이도
//   되겠지"라고 생각하면 안 된다. read 경로도 반드시 FReadScopeLock을
//   거쳐야 하고, 그래야 write와의 충돌을 RWLock이 정확히 감지해 막아준다.
//
// - RW lock이 항상 이기는 것은 아니다. write 비율이 높거나(예: 1:1에
//   가까움) critical section이 아주 짧으면, reader 수 카운팅과 writer
//   대기열 관리 비용 때문에 오히려 FCriticalSection보다 느릴 수 있다.
//   EquipmentCacheRWLock.cpp의 벤치마크로 이 방향성을 직접 실측해보고,
//   실제 프로젝트에서도 Unreal Insights로 확인한 뒤 도입 여부를 정한다.
//
// - 다음 학습 주제인 "starvation·fairness"와 바로 연결된다: read 요청이
//   끊임없이 들어오면 writer가 무한정 기다리게 될 수 있다(writer
//   starvation). 어떤 RW lock 구현은 "writer가 대기 중이면 새 reader를
//   잠시 막아 writer에게 순서를 준다"는 fairness 정책을 두기도 한다 -
//   FRWLock을 실제로 채택하기 전에 이 구현 세부사항도 확인해야 한다.
