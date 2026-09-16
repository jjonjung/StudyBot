// AtomicOrderingBenchmark.cpp
//
// 2026-09-16 학습 노트(2026-09-16_relaxed-vs-seqcst.md)에서 다룬
// memory_order_relaxed vs memory_order_seq_cst를 실측 가능한 코드로
// 확인하는 순수 C++ 벤치마크. 표준 C++(<atomic>)만 사용하므로 UE5 없이도
// 컴파일해서 직접 실행할 수 있다.
// (UE5 실전 버전은 AtomicOrderingExamples_UE5.h 참고)
//
// 컴파일 예:
//   g++ -O2 -std=c++17 -pthread AtomicOrderingBenchmark.cpp -o atomic_bench && ./atomic_bench
//   cl /O2 /std:c++17 /EHsc AtomicOrderingBenchmark.cpp
//
// 이 파일은 두 가지를 각각 실측한다.
//
//   [Part A] relaxed vs seq_cst 카운터 처리량 비교
//     여러 thread가 동시에 fetch_add하는 순수 통계 카운터에서, ordering을
//     강하게(seq_cst, 기본값) 쓸 때와 완화(relaxed)했을 때 처리량 차이가
//     실제로 얼마나 나는지 측정한다. "무조건 relaxed가 빠르다"가 아니라
//     "이 CPU/workload에서는 이만큼 차이난다"를 직접 확인하는 것이 목적이다.
//     두 경우 모두 최종 카운터 값은 항상 정확하다(atomicity는 ordering과
//     무관하게 항상 보장되기 때문) - 여기서 달라지는 건 오직 "속도"뿐이다.
//
//   [Part B] SB(Store Buffering) litmus test - 왜 seq_cst가 필요한가
//     두 개의 독립된 atomic 변수를 서로 다른 thread가 하나씩 store하고,
//     같은 thread가 곧바로 "상대방" 변수를 load하는 고전적인 메모리 모델
//     litmus test다. relaxed로는 CPU의 store buffer 때문에 "내가 방금
//     store한 값이 아직 다른 코어에 보이기 전에 내가 먼저 상대방 값을
//     읽는" 재배치가 실제로 일어날 수 있어서, 두 thread 모두 "상대방이
//     아직 store 안 했다"고 관찰하는 조합(R1==0 && R2==0)이 나타날 수
//     있다. 이는 sequential consistency 하에서는 정의상 불가능한
//     조합이므로, seq_cst로는 절대 관측되지 않아야 한다.
//     이건 2026-09-16 노트의 "서로 다른 atomic 변수 여러 개를 여러
//     thread가 조합해서 판단할 때 relaxed/release-acquire로는 부족하다"
//     는 원칙을 가장 작은 형태로 재현한 교과서적 예제다.
//
//     참고: x86은 store buffering을 허용하는 메모리 모델(x86-TSO)이라
//     이론적으로는 위반이 재현될 수 있지만, 실제로는 store buffer가
//     매우 빨리 비워지기 때문에 관측 빈도가 극히 낮거나(수십만~수백만
//     회 중 0~수 회) 아예 안 보일 수도 있다. ARM/Apple Silicon처럼
//     메모리 모델이 더 약한 CPU에서는 훨씬 자주 관측된다. "내 PC에서
//     0회가 나왔다"가 "relaxed로도 항상 안전하다"는 증명이 되지
//     않는다는 것 자체가 이 실험의 핵심 교훈이다.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

// AosVsSoa/PipeThermalScan.cpp, EquipmentCacheRWLock.cpp와 동일한 이유로
// 사용하는 UTF-8 콘솔 출력 헬퍼. 자세한 원리는 그 파일들 상단 주석 참고.
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

// =======================================================================
// Part A. relaxed vs seq_cst 카운터 처리량
// =======================================================================
//
// 노트에 나온 TotalHits 같은 "순수 통계 카운터"를 흉내낸다. 여러 thread가
// 동시에 fetch_add만 반복한다 - 다른 메모리와의 동기화는 전혀 필요 없는,
// relaxed의 전형적인 적합 사례다.
static double RunCounterBenchmark(std::memory_order Order, unsigned ThreadCount, uint64_t OpsPerThread)
{
    std::atomic<uint64_t> Counter{0};
    std::vector<std::thread> Workers;
    Workers.reserve(ThreadCount);

    const auto StartTime = std::chrono::high_resolution_clock::now();

    for (unsigned t = 0; t < ThreadCount; ++t)
    {
        Workers.emplace_back([&Counter, OpsPerThread, Order]()
        {
            for (uint64_t i = 0; i < OpsPerThread; ++i)
            {
                Counter.fetch_add(1, Order);
            }
        });
    }
    for (auto& W : Workers)
    {
        W.join();
    }

    const auto EndTime = std::chrono::high_resolution_clock::now();

    // ordering과 무관하게 값 자체는 항상 정확해야 한다(atomicity는 별개 보장).
    const uint64_t Expected = static_cast<uint64_t>(ThreadCount) * OpsPerThread;
    if (Counter.load() != Expected)
    {
        PrintfUtf8(u8"  [경고] 카운터 값이 예상과 다릅니다: %llu (기대값 %llu) - 이건 버그입니다.\n",
            static_cast<unsigned long long>(Counter.load()), static_cast<unsigned long long>(Expected));
    }

    return std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
}

// =======================================================================
// Part B. SB(Store Buffering) litmus test
// =======================================================================

struct FSbResult
{
    long ForbiddenCount = 0; // sequential consistency 하에서는 절대 나오면 안 되는 (0,0) 관측 횟수
    long TotalTrials = 0;
};

// 매 trial마다 두 thread를 새로 만들어 "거의 동시에" X.store/Y.load,
// Y.store/X.load를 실행시킨다. 매번 barrier로 두 thread를 최대한 같은
// 타이밍에 출발시켜야 재배치가 드러날 확률이 올라간다.
static FSbResult RunStoreBufferingLitmusTest(std::memory_order Order, long Trials)
{
    FSbResult Result;
    Result.TotalTrials = Trials;

    for (long trial = 0; trial < Trials; ++trial)
    {
        std::atomic<int> X{0};
        std::atomic<int> Y{0};
        std::atomic<int> ReadyCount{0};
        std::atomic<bool> Go{false};

        int R1 = -1;
        int R2 = -1;

        std::thread T1([&]()
        {
            ReadyCount.fetch_add(1, std::memory_order_relaxed);
            while (!Go.load(std::memory_order_relaxed)) {}
            X.store(1, Order);
            R1 = Y.load(Order);
        });

        std::thread T2([&]()
        {
            ReadyCount.fetch_add(1, std::memory_order_relaxed);
            while (!Go.load(std::memory_order_relaxed)) {}
            Y.store(1, Order);
            R2 = X.load(Order);
        });

        // 두 thread가 모두 준비될 때까지 기다렸다가 동시에 출발시킨다
        // (barrier 자체의 ordering은 실험 대상이 아니므로 relaxed로 충분하다).
        while (ReadyCount.load(std::memory_order_relaxed) < 2) {}
        Go.store(true, std::memory_order_relaxed);

        T1.join();
        T2.join();

        // 실제로 "순차적으로" 일어난 일이었다면 최소 한쪽은 상대방의
        // store를 봤어야 한다(R1==1 또는 R2==1). 둘 다 0이라는 건 "나는
        // 이미 store했는데 상대방은 아직 store 안 한 것처럼 보인다"가
        // 양쪽 모두에서 동시에 성립했다는 뜻 - sequential consistency
        // 에서는 불가능한 조합이다.
        if (R1 == 0 && R2 == 0)
        {
            ++Result.ForbiddenCount;
        }
    }

    return Result;
}

int main()
{
    const unsigned HwThreads = std::thread::hardware_concurrency();
    const unsigned ThreadCount = (HwThreads > 0) ? HwThreads : 8;
    const uint64_t OpsPerThread = 300000;
    const int Iterations = 5;

    PrintfUtf8(u8"=== Part A. relaxed vs seq_cst 카운터 처리량 ===\n");
    PrintfUtf8(u8"thread 수: %u, thread당 fetch_add 횟수: %llu\n\n",
        ThreadCount, static_cast<unsigned long long>(OpsPerThread));

    double RelaxedTotal = 0.0;
    double SeqCstTotal = 0.0;
    for (int it = 0; it < Iterations; ++it)
    {
        RelaxedTotal += RunCounterBenchmark(std::memory_order_relaxed, ThreadCount, OpsPerThread);
        SeqCstTotal  += RunCounterBenchmark(std::memory_order_seq_cst, ThreadCount, OpsPerThread);
    }

    PrintfUtf8(u8"[relaxed]  평균 소요 시간: %.4f ms\n", RelaxedTotal / Iterations);
    PrintfUtf8(u8"[seq_cst]  평균 소요 시간: %.4f ms\n", SeqCstTotal / Iterations);
    PrintfUtf8(u8"-> seq_cst/relaxed 비율: %.2fx (1.0보다 크면 relaxed가 더 빠른 것)\n\n",
        SeqCstTotal / (RelaxedTotal + 1e-9));
    PrintfUtf8(u8"해석: 이 비율은 CPU 아키텍처(특히 x86 vs ARM)에 따라 크게 달라진다.\n");
    PrintfUtf8(u8"      x86은 하드웨어 자체가 이미 강한 순서를 갖고 있어 차이가 작게\n");
    PrintfUtf8(u8"      나오는 경우가 많고, ARM/모바일 계열은 차이가 더 크게 나올 수\n");
    PrintfUtf8(u8"      있다. '얼마나 빠른가'보다 '이 카운터가 정말 relaxed로도 충분한\n");
    PrintfUtf8(u8"      값인가'를 먼저 판단하는 게 우선이다.\n\n");

    PrintfUtf8(u8"=== Part B. SB(Store Buffering) litmus test ===\n");
    const long Trials = 2000; // 재현 빈도를 더 보고 싶다면 늘려서 실행해봐도 좋다

    const FSbResult RelaxedResult = RunStoreBufferingLitmusTest(std::memory_order_relaxed, Trials);
    PrintfUtf8(u8"[relaxed]  %ld회 중 금지된 조합(R1==0 && R2==0) 관측: %ld회\n",
        RelaxedResult.TotalTrials, RelaxedResult.ForbiddenCount);

    const FSbResult SeqCstResult = RunStoreBufferingLitmusTest(std::memory_order_seq_cst, Trials);
    PrintfUtf8(u8"[seq_cst]  %ld회 중 금지된 조합(R1==0 && R2==0) 관측: %ld회 (이론상 항상 0)\n\n",
        SeqCstResult.TotalTrials, SeqCstResult.ForbiddenCount);

    PrintfUtf8(u8"해석: relaxed에서 0이 아닌 횟수가 관측됐다면, 이것이 바로 '서로 다른\n");
    PrintfUtf8(u8"      atomic 변수 여러 개를 여러 thread가 조합해서 판단할 때 relaxed로는\n");
    PrintfUtf8(u8"      부족하다'는 원칙이 실제 하드웨어에서 재현된 것이다. x86에서는 이\n");
    PrintfUtf8(u8"      값이 0으로 나올 수도 있는데(store buffer가 빨리 비워지는 경우가\n");
    PrintfUtf8(u8"      많음), 그렇다고 '이 패턴이 항상 안전하다'는 뜻은 아니다 - 다른\n");
    PrintfUtf8(u8"      CPU/컴파일러 최적화 수준/코어 배치에서는 언제든 다시 나타날 수\n");
    PrintfUtf8(u8"      있는, 표준이 허용하는 동작이기 때문이다. seq_cst에서는 이 조합이\n");
    PrintfUtf8(u8"      정의상 절대 나오지 않는다.\n");

    return 0;
}
