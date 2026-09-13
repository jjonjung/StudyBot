// EquipmentCacheRWLock.cpp
//
// 오늘 학습한 "FRWLock / Reader-Writer Lock" 개념을 반도체 팹(Fab) 설비
// 상태 캐시 조회 상황에 적용한 순수 C++ 벤치마크. 표준 C++(<shared_mutex>)
// 만 사용하므로 UE5 없이도 컴파일해서 직접 실행할 수 있다.
// (UE5 실전 버전은 EquipmentCacheRWLock_UE5.h 참고)
//
// 컴파일 예:
//   g++ -O2 -std=c++17 -pthread EquipmentCacheRWLock.cpp -o bench && ./bench
//   cl /O2 /std:c++17 /EHsc EquipmentCacheRWLock.cpp
//
// 개념 정리
// ---------
// 일반 mutex(std::mutex, UE의 FCriticalSection)는 "누가 들어왔든" 한 번에
// 한 thread만 critical section을 통과시킨다. Reader끼리도 서로를 막는다.
//
// 하지만 실무 데이터의 상당수는 "변경은 드물고 조회만 매우 잦다"는 패턴을
// 보인다 (설비 상태 캐시, 플레이어 스냅샷, 설정 테이블 등). 이런 경우
// reader끼리는 서로 막을 이유가 없다 - 다들 "읽기만" 하므로 데이터가
// 훼손될 위험이 없기 때문이다.
//
// Reader-Writer Lock(RW lock)은 이 관찰을 반영한 동기화 도구다.
//
//   Reader + Reader -> 동시 허용 가능 (둘 다 읽기만 하므로 안전)
//   Reader + Writer -> 충돌 (읽는 도중 값이 바뀌면 안 됨)
//   Writer + Writer -> 충돌 (동시에 쓰면 데이터 훼손)
//
// UE5에는 FRWLock과 RAII wrapper인 FReadScopeLock / FWriteScopeLock이
// 있다. 표준 C++에는 std::shared_mutex(C++17)와 std::shared_lock(읽기),
// std::unique_lock(쓰기)이 대응된다. 이 벤치마크는 표준 C++ 버전으로
// 같은 개념을 확인한다.
//
// 중요한 함정 두 가지
// --------------------
// 1. "read lock을 쓴다고 컨테이너가 자동으로 thread-safe해지는 것은
//    아니다." 모든 접근 경로가 같은 lock 규칙을 지켜야 한다. 단 한
//    곳이라도 lock 없이 write하면 보호 모델 전체가 깨진다.
// 2. "RW lock이 항상 mutex보다 빠른 것도 아니다." write 비율이 높거나
//    critical section이 아주 짧으면, RW lock 자체의 관리 비용(reader
//    수 카운팅, writer 대기열 관리 등) 때문에 오히려 일반 mutex보다
//    느릴 수 있다. 그래서 "read 비율이 압도적으로 높을 때"만 유리하다.
//
// 실험 설계
// ---------
// 같은 캐시(설비 ID -> 상태)에 대해 "read 20 : write 1" 비율로 접근하는
// 다수 thread를 돌리고, 잠금 방식만 바꿔가며 총 소요 시간을 측정한다.
//
//   [실험 A] std::mutex           - 모든 접근(read/write 구분 없이) 직렬화
//   [실험 B] std::shared_mutex    - read끼리는 동시 허용, write만 배타적
//
// 두 실험 모두 "총 접근 횟수"와 "read:write 비율"은 동일하게 맞춘다.
// read가 압도적으로 많은 workload에서 B가 A보다 빨라야 "reader를 동시에
// 허용한 것"이 실제로 이득이 되는 상황임을 확인할 수 있다.

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <random>
#include <atomic>

#if defined(_WIN32)
#include <windows.h>
#endif

// AosVsSoa/PipeThermalScan.cpp와 동일한 이유로 사용하는 UTF-8 콘솔 출력
// 헬퍼. 자세한 원리는 그 파일 상단 주석 참고.
static void PrintUtf8(const char* Utf8Text)
{
#if defined(_WIN32)
    HANDLE Out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD ConsoleMode = 0;
    const bool bIsConsole = (Out != nullptr) && (Out != INVALID_HANDLE_VALUE) && GetConsoleMode(Out, &ConsoleMode);

    if (bIsConsole)
    {
        const int WideLen = MultiByteToWideChar(CP_UTF8, 0, Utf8Text, -1, nullptr, 0);
        if (WideLen > 0)
        {
            std::vector<wchar_t> Wide(static_cast<size_t>(WideLen));
            MultiByteToWideChar(CP_UTF8, 0, Utf8Text, -1, Wide.data(), WideLen);
            DWORD Written = 0;
            WriteConsoleW(Out, Wide.data(), static_cast<DWORD>(WideLen - 1), &Written, nullptr);
        }
    }
    else
    {
        std::fputs(Utf8Text, stdout);
    }
#else
    std::fputs(Utf8Text, stdout);
#endif
}

template <typename... Args>
static void PrintfUtf8(const char* Utf8Fmt, Args... args)
{
    char Buffer[512];
    std::snprintf(Buffer, sizeof(Buffer), Utf8Fmt, args...);
    PrintUtf8(Buffer);
}

// ---------------------------------------------------------------------
// 설비 상태: 반도체 팹의 설비(Equipment) 하나의 스냅샷. 실제로는 온도,
// 압력, 가동 여부 등 여러 필드가 있겠지만, 이 벤치마크의 핵심은 "락
// 방식에 따른 처리량 차이"이므로 필드는 최소화했다.
struct FEquipmentState
{
    float Temperature = 20.0f;
    float Pressure = 1.0f;
    uint64_t Version = 0;
};

constexpr size_t EquipmentCount = 2000;

// ---------------------------------------------------------------------
// [실험 A] std::mutex 버전 - read/write 구분 없이 전부 직렬화
// ---------------------------------------------------------------------
class FEquipmentCache_Mutex
{
public:
    FEquipmentCache_Mutex()
    {
        States.resize(EquipmentCount);
    }

    // 나쁜 예에 가까운 상황: read 요청조차 write와 동일한 배타적 lock을
    // 잡는다. read끼리도 서로를 기다리게 만든다.
    // 인접한 여러 설비를 한 번에 조회(예: 같은 Zone 대시보드 갱신)하는
    // 상황을 흉내낸다. read 하나가 값을 여러 개 들여다볼수록 critical
    // section이 길어지고, RW lock의 "reader 동시 허용" 이득이 실측으로
    // 드러나기 쉬워진다.
    uint64_t ReadZone(size_t EquipmentId, size_t ZoneSize) const
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        uint64_t Sum = 0;
        const size_t End = (EquipmentId + ZoneSize < States.size()) ? EquipmentId + ZoneSize : States.size();
        for (size_t i = EquipmentId; i < End; ++i)
        {
            Sum += States[i].Version;
        }
        return Sum;
    }

    void Write(size_t EquipmentId, float NewTemperature, float NewPressure)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        FEquipmentState& State = States[EquipmentId];
        State.Temperature = NewTemperature;
        State.Pressure = NewPressure;
        ++State.Version;
    }

private:
    mutable std::mutex Mutex;
    std::vector<FEquipmentState> States;
};

// ---------------------------------------------------------------------
// [실험 B] std::shared_mutex 버전 - reader끼리는 동시 허용, writer만 배타적
// ---------------------------------------------------------------------
class FEquipmentCache_RWLock
{
public:
    FEquipmentCache_RWLock()
    {
        States.resize(EquipmentCount);
    }

    // std::shared_lock == UE의 FReadScopeLock에 해당. 여러 thread가
    // 동시에 이 lock을 "공유(shared)" 모드로 잡을 수 있다.
    uint64_t ReadZone(size_t EquipmentId, size_t ZoneSize) const
    {
        std::shared_lock<std::shared_mutex> Lock(Mutex);
        uint64_t Sum = 0;
        const size_t End = (EquipmentId + ZoneSize < States.size()) ? EquipmentId + ZoneSize : States.size();
        for (size_t i = EquipmentId; i < End; ++i)
        {
            Sum += States[i].Version;
        }
        return Sum;
    }

    // std::unique_lock == UE의 FWriteScopeLock에 해당. 이 lock을 잡는
    // 동안에는 다른 reader/writer 모두 대기해야 한다(배타적).
    void Write(size_t EquipmentId, float NewTemperature, float NewPressure)
    {
        std::unique_lock<std::shared_mutex> Lock(Mutex);
        FEquipmentState& State = States[EquipmentId];
        State.Temperature = NewTemperature;
        State.Pressure = NewPressure;
        ++State.Version;
    }

private:
    mutable std::shared_mutex Mutex;
    std::vector<FEquipmentState> States;
};

// ---------------------------------------------------------------------
// 공통 워크로드: read:write = ReadWeight:1 비율로 총 OpsPerThread번 접근.
// CacheType은 FEquipmentCache_Mutex 또는 FEquipmentCache_RWLock.
// ---------------------------------------------------------------------
template <typename CacheType>
static double RunWorkload(CacheType& Cache, unsigned ThreadCount, size_t OpsPerThread, unsigned ReadWeight, size_t ZoneSize)
{
    std::vector<std::thread> Workers;
    Workers.reserve(ThreadCount);
    std::atomic<uint64_t> Sink{0}; // 컴파일러가 Read 결과를 최적화로 없애지 못하게 막는 용도

    const auto StartTime = std::chrono::high_resolution_clock::now();

    for (unsigned t = 0; t < ThreadCount; ++t)
    {
        Workers.emplace_back([&Cache, &Sink, OpsPerThread, ReadWeight, ZoneSize, t]()
        {
            std::mt19937 Rng(1000u + t);
            std::uniform_int_distribution<size_t> IdDist(0, EquipmentCount - 1 - ZoneSize);
            std::uniform_int_distribution<unsigned> OpDist(0, ReadWeight); // 0..ReadWeight, 0이면 write

            for (size_t i = 0; i < OpsPerThread; ++i)
            {
                const size_t Id = IdDist(Rng);
                if (OpDist(Rng) == 0)
                {
                    Cache.Write(Id, 20.0f + static_cast<float>(i % 10), 1.0f);
                }
                else
                {
                    const uint64_t Sum = Cache.ReadZone(Id, ZoneSize);
                    Sink.fetch_add(Sum, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& W : Workers)
    {
        W.join();
    }

    const auto EndTime = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
}

int main()
{
    const unsigned HwThreads = std::thread::hardware_concurrency();
    const unsigned ThreadCount = (HwThreads > 0) ? HwThreads : 8;
    const size_t OpsPerThread = 50000;
    const unsigned ReadWeight = 200; // read:write = 200:1 (강한 read-heavy workload)
    const size_t ZoneSize = 256;     // read 한 번에 인접 설비 256개를 함께 조회 (critical section을 충분히 늘림)
    const int Iterations = 5;

    PrintfUtf8(u8"설비 수: %zu, thread 수: %u, thread당 접근 횟수: %zu, read:write = %u:1, zone 크기: %zu\n\n",
        EquipmentCount, ThreadCount, OpsPerThread, ReadWeight, ZoneSize);

    // ---- 실험 A: std::mutex (read도 배타적으로 직렬화) ----
    double MutexTotal = 0.0;
    for (int it = 0; it < Iterations; ++it)
    {
        FEquipmentCache_Mutex Cache;
        MutexTotal += RunWorkload(Cache, ThreadCount, OpsPerThread, ReadWeight, ZoneSize);
    }
    PrintfUtf8(u8"[실험 A] std::mutex (reader도 서로 막음)\n");
    PrintfUtf8(u8"  평균 소요 시간: %.4f ms\n\n", MutexTotal / Iterations);

    // ---- 실험 B: std::shared_mutex (read끼리 동시 허용) ----
    double RWTotal = 0.0;
    for (int it = 0; it < Iterations; ++it)
    {
        FEquipmentCache_RWLock Cache;
        RWTotal += RunWorkload(Cache, ThreadCount, OpsPerThread, ReadWeight, ZoneSize);
    }
    PrintfUtf8(u8"[실험 B] std::shared_mutex (reader끼리 동시 허용)\n");
    PrintfUtf8(u8"  평균 소요 시간: %.4f ms\n\n", RWTotal / Iterations);

    const double Ratio = MutexTotal / (RWTotal + 1e-9);
    PrintfUtf8(u8"-> Mutex/RWLock 비율: %.2fx (1.0보다 크면 RW lock이 더 빠른 것)\n\n", Ratio);

    PrintfUtf8(u8"해석: read:write 비율이 %u:1로 read-heavy한 workload에서는, reader끼리\n", ReadWeight);
    PrintfUtf8(u8"      서로 기다릴 필요가 없는 shared_mutex가 유리하게 나오는 경향이 있다.\n");
    PrintfUtf8(u8"      단, 이 비율은 read:write 비율, critical section 길이, thread 수,\n");
    PrintfUtf8(u8"      OS/컴파일러에 따라 달라진다 - write 비율을 높이거나(예: 1:1)\n");
    PrintfUtf8(u8"      critical section을 아주 짧게 만들면 RW lock의 관리 비용(reader 수\n");
    PrintfUtf8(u8"      카운팅, writer 대기열 처리) 때문에 오히려 mutex보다 느려질 수도\n");
    PrintfUtf8(u8"      있다. '항상 RW lock이 이긴다'가 아니라 '이 workload에서 실측해보니\n");
    PrintfUtf8(u8"      이렇다'는 방향으로 받아들여야 한다.\n");

    return 0;
}
