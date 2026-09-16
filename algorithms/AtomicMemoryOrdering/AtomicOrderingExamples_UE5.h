// AtomicOrderingExamples_UE5.h
//
// AtomicOrderingBenchmark.cpp(순수 C++, std::atomic)와 동일한 "relaxed vs
// seq_cst" 개념을, UE5 실전 코드로 옮긴 버전. std::atomic은 UE5 C++
// 프로젝트에서도 그대로 사용할 수 있으며(엔진 자체 컨테이너/객체가 아닌
// 순수 값 하나를 원자적으로 다룰 때는 std::atomic이 흔히 쓰인다), 여기서는
// 2026-09-16 노트에서 다룬 두 가지 패턴을 UE5 문맥으로 재구성한다.
//
//   - 순수 통계 카운터            -> memory_order_relaxed로 충분
//   - 서로 다른 두 상태 플래그의 "동시 성립" 판정 -> memory_order_seq_cst 필요
//
// 두 가지 도메인 예시를 함께 담았다(EquipmentCacheRWLock_UE5.h와 동일한
// 구성 방식).
//   1) AQUALOS(협동 액션 게임): 여러 플레이어 thread가 보스에게 가한
//      히트를 누적하고, 서로 다른 thread가 설정하는 두 상태 플래그의
//      조합으로 페이즈 전환을 판정한다.
//   2) 반도체 Fab 자동화: 설비 컨트롤러가 처리한 lot 수를 누적하고,
//      서로 다른 알람 플래그의 조합으로 비상정지(E-stop)를 판정한다.

#pragma once

#include "CoreMinimal.h"
#include <atomic>

// =======================================================================
// 패턴 1 — 순수 통계 카운터: memory_order_relaxed로 충분
// =======================================================================
//
// 이 값을 "봤다"는 사실이 다른 메모리의 준비 상태를 알리는 신호로 쓰이지
// 않는다. 여러 thread가 동시에 증가시켜도 값이 유실되지 않기만 하면
// 되므로 relaxed가 자연스러운 선택이다.

// ----- 1) 게임: 대왕문어 보스에게 가한 누적 히트 카운트 -----
class FBossHitCounter
{
public:
    // 여러 플레이어의 공격 처리 thread가 동시에 호출해도 안전하다.
    void RecordHit()
    {
        TotalHits.fetch_add(1, std::memory_order_relaxed);
    }

    uint64 GetTotalHits() const
    {
        return TotalHits.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64> TotalHits{0};
};

// ----- 2) 반도체 Fab: 설비가 처리한 누적 lot 수 -----
class FEquipmentLotThroughputCounter
{
public:
    // 설비 컨트롤러 thread가 lot 하나의 공정을 끝낼 때마다 호출.
    void RecordLotCompleted()
    {
        TotalLotsCompleted.fetch_add(1, std::memory_order_relaxed);
    }

    // 모니터링/대시보드 thread가 주기적으로 조회.
    uint64 GetTotalLotsCompleted() const
    {
        return TotalLotsCompleted.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64> TotalLotsCompleted{0};
};

// =======================================================================
// 패턴 2 — 서로 다른 두 플래그의 "동시 성립" 판정: memory_order_seq_cst 필요
// =======================================================================
//
// 두 개의 독립된 atomic 변수를 서로 다른 thread가 하나씩 설정하고, 제3의
// thread가 "둘 다 true인 조합"을 근거로 중요한 동작(연출 트리거, 비상정지
// 등)을 실행한다. 서로 다른 atomic 변수 사이의 순서는 release-acquire
// 로도 보장되지 않으므로, 여러 스레드가 여러 atomic을 조합해서 판단해야
// 하는 상황에서는 seq_cst로 하나의 전역 순서를 강제해야 한다.
// (근거: AtomicOrderingBenchmark.cpp의 Part B, SB litmus test)

// ----- 1) 게임: 보스 페이즈 전환 조건 (환경 위험 + 보스 분노) -----
class FBossPhaseFlags_Bad
{
public:
    // [환경 thread]
    void SetArenaHazardActive()
    {
        // 위험: relaxed는 이 store와 다른 atomic 변수 사이의 전역 순서를
        // 보장하지 않는다.
        bArenaHazardActive.store(true, std::memory_order_relaxed);
    }

    // [보스 AI thread]
    void SetBossEnraged()
    {
        bBossEnraged.store(true, std::memory_order_relaxed);
    }

    // [이벤트 매니저 thread] 두 조건이 "동시에" true인 순간을 놓칠 수 있다.
    bool ShouldTriggerPhaseTransition() const
    {
        const bool bHazard = bArenaHazardActive.load(std::memory_order_relaxed);
        const bool bEnraged = bBossEnraged.load(std::memory_order_relaxed);
        return bHazard && bEnraged;
    }

private:
    std::atomic<bool> bArenaHazardActive{false};
    std::atomic<bool> bBossEnraged{false};
};

class FBossPhaseFlags_Good
{
public:
    void SetArenaHazardActive()
    {
        bArenaHazardActive.store(true, std::memory_order_seq_cst);
    }

    void SetBossEnraged()
    {
        bBossEnraged.store(true, std::memory_order_seq_cst);
    }

    bool ShouldTriggerPhaseTransition() const
    {
        const bool bHazard = bArenaHazardActive.load(std::memory_order_seq_cst);
        const bool bEnraged = bBossEnraged.load(std::memory_order_seq_cst);
        return bHazard && bEnraged;
    }

private:
    std::atomic<bool> bArenaHazardActive{false};
    std::atomic<bool> bBossEnraged{false};
};

// ----- 2) 반도체 Fab: 비상정지(E-stop) 조건 (온도 알람 + 압력 알람) -----
class FEquipmentAlarmFlags_Bad
{
public:
    // [온도 센서 감시 thread]
    void RaiseTemperatureAlarm()
    {
        bTemperatureAlarm.store(true, std::memory_order_relaxed);
    }

    // [압력 센서 감시 thread]
    void RaisePressureAlarm()
    {
        bPressureAlarm.store(true, std::memory_order_relaxed);
    }

    // [안전 watchdog thread] false negative(동시 발생을 놓치는 것)가
    // 안전 사고로 이어질 수 있는 판정 로직인데도 relaxed를 쓰고 있다.
    bool ShouldTriggerEStop() const
    {
        const bool bTemp = bTemperatureAlarm.load(std::memory_order_relaxed);
        const bool bPressure = bPressureAlarm.load(std::memory_order_relaxed);
        return bTemp && bPressure;
    }

private:
    std::atomic<bool> bTemperatureAlarm{false};
    std::atomic<bool> bPressureAlarm{false};
};

class FEquipmentAlarmFlags_Good
{
public:
    void RaiseTemperatureAlarm()
    {
        bTemperatureAlarm.store(true, std::memory_order_seq_cst);
    }

    void RaisePressureAlarm()
    {
        bPressureAlarm.store(true, std::memory_order_seq_cst);
    }

    // 안전에 직결되는 판정이므로 성능보다 correctness를 우선한다.
    bool ShouldTriggerEStop() const
    {
        const bool bTemp = bTemperatureAlarm.load(std::memory_order_seq_cst);
        const bool bPressure = bPressureAlarm.load(std::memory_order_seq_cst);
        return bTemp && bPressure;
    }

private:
    std::atomic<bool> bTemperatureAlarm{false};
    std::atomic<bool> bPressureAlarm{false};
};

// ---------------------------------------------------------------------
// 선택 가이드 (오늘 학습 결론을 이 도메인에 맞게 정리)
// ---------------------------------------------------------------------
//
// - FBossHitCounter / FEquipmentLotThroughputCounter처럼 "그 값 자체"만
//   중요하고 다른 메모리의 신호로 쓰이지 않는 순수 카운터는 relaxed로
//   충분하다. 여기에 seq_cst를 쓰는 건 틀린 건 아니지만 불필요한 비용이다.
//
// - FBossPhaseFlags_Bad / FEquipmentAlarmFlags_Bad처럼 "서로 다른 두 개
//   이상의 atomic 변수를 여러 thread가 조합해서 판단"하는 로직에는
//   relaxed는 물론 release-acquire조차 부족할 수 있다(release-acquire는
//   "하나의 변수를 매개로 한" 동기화만 보장하기 때문). 이런 곳은 반드시
//   seq_cst(또는 명시적 fence)로 전역 순서를 확보해야 한다.
//
// - 특히 FEquipmentAlarmFlags처럼 "놓치면 안전 사고로 이어지는" 판정
//   로직은 프로파일링으로 병목이 확인되기 전까지는 성능 최적화를
//   시도하지 않는 게 맞다 - correctness가 항상 우선이다.
//
// - 어떤 atomic 변수가 A그룹(relaxed 후보)인지 B그룹(release-acquire/
//   seq_cst 후보)인지 확신이 서지 않는다면, 일단 seq_cst(std::atomic의
//   기본값)로 두고 프로파일링에서 실제 병목으로 확인될 때만 완화를
//   검토한다. AtomicOrderingBenchmark.cpp의 Part A/B가 그 판단에
//   필요한 실측 방법(처리량 비교, litmus test)을 보여준다.
//
// - 다음 학습 주제인 "atomic RMW(read-modify-write)와 ABA 문제"는 이
//   두 클래스보다 한 단계 더 복잡한 "compare-and-swap 기반 lock-free
//   자료구조"로 바로 이어진다.
