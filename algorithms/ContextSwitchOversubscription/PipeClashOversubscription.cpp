// PipeClashOversubscription.cpp
//
// 오늘 학습한 "context switch와 oversubscription" 개념을 반도체 팹 배관
// CAD 도면의 "배관 간섭 검사(clash detection)" 상황에 적용한 순수 C++
// 벤치마크. 표준 C++(<thread>)만 사용하므로 UE5 없이도 컴파일해서 직접
// 실행할 수 있다. (UE5 실전 버전은 PipeClashOversubscription_UE5.h 참고)
//
// 컴파일 예:
//   g++ -O2 -std=c++17 -pthread PipeClashOversubscription.cpp -o bench && ./bench
//   cl /O2 /std:c++17 /EHsc PipeClashOversubscription.cpp
//
// 개념 정리
// ---------
// OS scheduler는 실행 가능한 여러 thread 중 어떤 thread를 CPU에서 실행할지
// 결정한다. 현재 thread가 시간 할당량을 다 쓰거나, 더 높은 우선순위 thread가
// 준비되거나, I/O·동기화를 기다리게 되면 다른 thread로 실행 주체가 바뀔 수
// 있다 (context switch).
//
//   현재 thread 상태 저장 -> 다음 실행 thread 선택 -> 다음 thread 상태 복원
//   -> 실행 재개
//
// 이 과정의 비용은 레지스터 저장/복원만이 아니다. 다른 thread가 실행되는
// 동안 기존 thread가 데워둔 cache/TLB 내용이 밀려날 수 있고, 다시 돌아왔을
// 때 그 데이터가 cache에 남아 있지 않아 다시 메모리에서 채워야 할 수 있다
// (cache 재가열 비용).
//
// Oversubscription = CPU가 실질적으로 동시에 처리할 수 있는 hardware thread
// 수보다, 활발히 실행하려는 software thread 수가 훨씬 많은 상태. 예를 들어
//
//   8개 hardware thread, 30개 CPU-bound worker thread
//
// 라면 30개가 동시에 도는 게 아니라 일부만 돌고 나머지는 대기·교체된다.
// 그래서 "thread 수를 늘리면 병렬 성능도 늘어난다"는 성립하지 않는다 -
// 오히려 CPU-bound 작업에서는 hardware_concurrency() 근처의 thread 수가
// 총 처리 시간이 가장 짧고, 그보다 훨씬 많이 만들면 context switch와 cache
// 재가열 비용 때문에 총 처리 시간이 늘어날 수 있다.
//
// 이번 예제 상황
// --------------
// 반도체 팹 배관 CAD 도면에는 수만 개의 배관 세그먼트가 있고, 설계
// 변경 후 "새로 옮긴 배관들이 기존 배관·구조물과 간섭(clash)하지 않는지"
// 검사해야 한다. 배관 세그먼트 하나당 다른 세그먼트들과의 거리를 계산하는
// clash detection은 전형적인 CPU-bound 작업이다.
//
// 도면을 N개의 구역(zone)으로 나눠 구역별로 worker thread를 하나씩 만들어
// 병렬로 검사한다고 하자. 이때 구역 수(=thread 수)를 어떻게 정해야 할까?
// "구역을 잘게 쪼갤수록 빠르다"고 생각하기 쉽지만, 실제로는 hardware
// thread 수를 한참 넘어서면 오히려 느려질 수 있다는 것을 실측으로
// 확인해본다.
//
// 실험 설계
// ---------
// 같은 총 작업량(배관 세그먼트 전체에 대한 clash detection, 인위적으로
// CPU를 소모하는 순수 연산)을 thread 수만 바꿔가며 나눠 처리하고 총
// 소요 시간을 측정한다.
//
//   [실험 A] thread 수 = hardware_concurrency()           (적정 구독)
//   [실험 B] thread 수 = hardware_concurrency() x 8        (심한 oversubscription)
//
// 두 실험 모두 "worker가 처리하는 총 연산량"은 동일하게 맞춘다 - thread
// 수가 늘어난 만큼 thread 하나가 처리하는 몫은 줄어든다. 따라서 이상적인
// (context switch 비용이 0인) 세계라면 두 실험의 총 소요 시간은 비슷해야
// 한다. 실측 차이가 있다면 그것이 곧 "과도한 thread 생성/교체 비용"이다.

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <random>

#if defined(_WIN32)
#include <windows.h>
#endif

// AosVsSoa/PipeThermalScan.cpp와 동일한 이유로 사용하는 UTF-8 콘솔 출력
// 헬퍼. u8"..." 리터럴로 UTF-8 바이트를 표준 보장대로 확보한 뒤
// WriteConsoleW로 직접 콘솔에 쓰고, 콘솔이 아니면(리다이렉트) UTF-8 바이트를
// 그대로 내보낸다. 자세한 원리는 AosVsSoa/PipeThermalScan.cpp 상단 참고.
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
// 배관 세그먼트: 3D 좌표만 가진 단순 구조체. clash detection의 핵심은
// "세그먼트 하나와 나머지 세그먼트들 사이의 거리 계산"이라는 순수 CPU
// 연산이므로, 실제 CAD 자료구조 대신 좌표 배열로 단순화했다.
struct FPipeSegment
{
    float X, Y, Z;
};

static std::vector<FPipeSegment> MakeRandomPipes(size_t Count, uint32_t Seed)
{
    std::vector<FPipeSegment> Pipes(Count);
    std::mt19937 Rng(Seed);
    std::uniform_real_distribution<float> Dist(0.0f, 1000.0f);
    for (auto& Seg : Pipes)
    {
        Seg.X = Dist(Rng);
        Seg.Y = Dist(Rng);
        Seg.Z = Dist(Rng);
    }
    return Pipes;
}

// 세그먼트 하나가 담당 범위 안의 다른 세그먼트들과 거리를 계산해 임계치
// 이내(=간섭)인 쌍의 개수를 센다. 실제 clash detection보다 훨씬 단순하지만
// "CPU를 실제로 소모하는 연산"이라는 성격은 동일하다 - 여기서 중요한 건
// 알고리즘의 정교함이 아니라 "이 연산을 몇 개의 thread로 나눠 돌리는가"이다.
static uint64_t CountClashesInRange(const std::vector<FPipeSegment>& Pipes, size_t Begin, size_t End, float ThresholdSq)
{
    uint64_t ClashCount = 0;
    const size_t N = Pipes.size();
    for (size_t i = Begin; i < End; ++i)
    {
        const FPipeSegment& A = Pipes[i];
        // 간단화를 위해 각 세그먼트는 자기 뒤 64개 세그먼트까지만 검사한다
        // (실제로는 공간 분할 구조로 후보를 좁히는 것에 해당) - 이렇게 해야
        // 세그먼트 수를 늘려도 총 연산량이 O(N)에 가깝게 유지되어, thread
        // 수 변화에 따른 순수 스케줄링 비용 차이를 관찰하기 쉬워진다.
        const size_t Window = 64;
        const size_t Last = (i + Window < N) ? (i + Window) : N;
        for (size_t j = i + 1; j < Last; ++j)
        {
            const FPipeSegment& B = Pipes[j];
            const float dx = A.X - B.X;
            const float dy = A.Y - B.Y;
            const float dz = A.Z - B.Z;
            const float DistSq = dx * dx + dy * dy + dz * dz;
            if (DistSq < ThresholdSq)
            {
                ++ClashCount;
            }
        }
    }
    return ClashCount;
}

// 총 작업(전체 배관 세그먼트에 대한 clash detection)을 ThreadCount개의
// worker thread로 균등 분할해 실행하고, 전체가 끝날 때까지 걸린 시간(ms)을
// 반환한다. thread 수만 바꿔가며 이 함수를 호출하는 것이 이번 실험의 핵심.
static double RunClashDetectionParallel(const std::vector<FPipeSegment>& Pipes, unsigned ThreadCount, float ThresholdSq, uint64_t& OutClashTotal)
{
    const size_t N = Pipes.size();
    std::vector<std::thread> Workers;
    Workers.reserve(ThreadCount);
    std::vector<uint64_t> PartialCounts(ThreadCount, 0);

    const auto StartTime = std::chrono::high_resolution_clock::now();

    for (unsigned t = 0; t < ThreadCount; ++t)
    {
        const size_t Begin = (N * t) / ThreadCount;
        const size_t End = (N * (t + 1)) / ThreadCount;
        Workers.emplace_back([&Pipes, Begin, End, ThresholdSq, &PartialCounts, t]()
        {
            PartialCounts[t] = CountClashesInRange(Pipes, Begin, End, ThresholdSq);
        });
    }

    for (auto& W : Workers)
    {
        W.join();
    }

    const auto EndTime = std::chrono::high_resolution_clock::now();

    uint64_t Total = 0;
    for (uint64_t C : PartialCounts)
    {
        Total += C;
    }
    OutClashTotal = Total;

    return std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
}

int main()
{
    const size_t PipeCount = 200000;
    const int Iterations = 10;
    const float ThresholdSq = 25.0f; // 거리 5.0 이내를 "간섭"으로 간주 (좌표 범위 0~1000 기준)

    const unsigned HwThreads = std::thread::hardware_concurrency();
    const unsigned NormalThreadCount = (HwThreads > 0) ? HwThreads : 4;
    const unsigned OversubscribedThreadCount = NormalThreadCount * 8;

    std::vector<FPipeSegment> Pipes = MakeRandomPipes(PipeCount, /*Seed=*/12345);

    PrintfUtf8(u8"배관 세그먼트 수: %zu, 반복 횟수: %d\n", PipeCount, Iterations);
    PrintfUtf8(u8"이 머신의 hardware_concurrency(): %u\n\n", HwThreads);

    // ---- 실험 A: 적정 구독 (thread 수 = hardware_concurrency) ----
    double NormalTotal = 0.0;
    uint64_t NormalClashSink = 0;
    for (int it = 0; it < Iterations; ++it)
    {
        uint64_t ClashCount = 0;
        NormalTotal += RunClashDetectionParallel(Pipes, NormalThreadCount, ThresholdSq, ClashCount);
        NormalClashSink += ClashCount;
    }

    PrintfUtf8(u8"[실험 A] thread 수 = %u (hardware_concurrency 그대로)\n", NormalThreadCount);
    PrintfUtf8(u8"  평균 소요 시간: %.4f ms  (간섭 쌍=%llu)\n\n", NormalTotal / Iterations,
        (unsigned long long)(NormalClashSink / Iterations));

    // ---- 실험 B: 심한 oversubscription (thread 수 = hardware_concurrency x 8) ----
    double OverTotal = 0.0;
    uint64_t OverClashSink = 0;
    for (int it = 0; it < Iterations; ++it)
    {
        uint64_t ClashCount = 0;
        OverTotal += RunClashDetectionParallel(Pipes, OversubscribedThreadCount, ThresholdSq, ClashCount);
        OverClashSink += ClashCount;
    }

    PrintfUtf8(u8"[실험 B] thread 수 = %u (hardware_concurrency x 8, 심한 oversubscription)\n", OversubscribedThreadCount);
    PrintfUtf8(u8"  평균 소요 시간: %.4f ms  (간섭 쌍=%llu)\n\n", OverTotal / Iterations,
        (unsigned long long)(OverClashSink / Iterations));

    const double Ratio = OverTotal / (NormalTotal + 1e-9);
    PrintfUtf8(u8"-> Oversubscribed/Normal 비율: %.2fx (1.0보다 크면 thread를 과도하게 늘렸을 때 더 느려진 것)\n\n", Ratio);

    PrintfUtf8(u8"해석: 두 실험 모두 '총 연산량'은 동일하고 thread 수만 다르다. 이상적으로는\n");
    PrintfUtf8(u8"      thread 수가 늘어도 총 소요 시간은 비슷해야 하지만, 실제로는 hardware\n");
    PrintfUtf8(u8"      thread 수를 한참 넘겨 thread를 만들면 OS가 더 자주 context switch를\n");
    PrintfUtf8(u8"      해야 하고, 그때마다 다른 thread가 데워둔 cache/TLB 때문에 재가열 비용이\n");
    PrintfUtf8(u8"      발생한다. 'thread 수 증가 = 병렬 성능 증가'가 아니라는 것을 이 비율로\n");
    PrintfUtf8(u8"      확인할 수 있다. 정확한 배율은 CPU 코어 수·OS 스케줄러·부하 상황에 따라\n");
    PrintfUtf8(u8"      달라지므로, 이 수치 자체보다 '방향성'을 보는 용도로 삼아야 한다.\n");

    return 0;
}
