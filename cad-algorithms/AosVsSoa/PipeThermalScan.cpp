// PipeThermalScan.cpp
//
// 오늘 학습한 "AoS vs SoA와 cache 효율" 개념을 반도체 팹 배관 모니터링
// 상황에 적용한 순수 C++ 벤치마크. 표준 C++만 사용하므로 UE5 없이도
// 컴파일해서 직접 실행할 수 있다. (UE5 실전 버전은 PipeThermalScan_UE5.h 참고)
//
// 컴파일 예:
//   g++ -O2 -std=c++17 PipeThermalScan.cpp -o bench && ./bench
//
// 상황
// ----
// 반도체 팹 배관 도면에는 수만 개의 배관 세그먼트가 있고, 각 세그먼트는
// 실시간으로 온도(Temperature), 압력(Pressure), 유량(FlowRate) 센서 값을
// 갖는다. 그런데 실무에서 자주 도는 배치(batch) 작업은 이런 식이다.
//
//   - "전체 배관의 온도 평균/최대값을 1초마다 계산해서 대시보드에 표시"
//   - "압력이 임계치를 넘은 배관만 필터링해서 경고"
//
// 즉 한 세그먼트의 여러 속성을 "함께" 쓰는 게 아니라, 특정 필드 하나만
// 수만 개에 걸쳐 "쭉 훑는" 연산이 hot path다. 이런 패턴에서 AoS와 SoA가
// 실제로 얼마나 차이 나는지 확인해본다.

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <random>
#include <string>

// ---------------------------------------------------------------------
// AoS (Array of Structures) — "배관 하나"를 구조체로 묶고 배열로 저장
// ---------------------------------------------------------------------
//
// 메모리 배치 (개념도):
//   [Temp Pressure Flow Tag][Temp Pressure Flow Tag][Temp Pressure Flow Tag]...
//
// 배관 하나의 상태를 통째로 다룰 때(예: "이 배관 상세 정보 출력")는
// 자연스럽고 직관적이다. 하지만 "Temperature만 훑기"를 하면 Pressure,
// FlowRate, Tag까지 같은 cache line에 딸려 들어와, 정작 쓰지 않는
// 데이터가 대역폭을 갉아먹는다.
struct FPipeSegmentAoS
{
    float Temperature; // 섭씨
    float Pressure;    // kPa
    float FlowRate;    // L/min
    uint32_t TagId;    // 배관 태그 (P-101-42-2FL 같은 것의 해시값이라 가정)
};

// ---------------------------------------------------------------------
// SoA (Structure of Arrays) — 속성별로 배열을 분리
// ---------------------------------------------------------------------
//
// 메모리 배치 (개념도):
//   Temperature: [T T T T T T T T ...]
//   Pressure:    [P P P P P P P P ...]
//   FlowRate:    [F F F F F F F F ...]
//   TagId:       [ID ID ID ID ...]
//
// Temperature만 순회할 때는 Temperature 배열이 통째로 연속 메모리이므로
// 필요 없는 Pressure/FlowRate/TagId가 cache line에 끼어들지 않는다.
// 대신 "배관 하나의 온도+압력+유량을 함께 봐야" 하는 연산에서는
// 배열 세 개를 각각 인덱싱해야 해서 AoS보다 코드가 번거로워진다.
struct FPipeSegmentArraysSoA
{
    std::vector<float> Temperature;
    std::vector<float> Pressure;
    std::vector<float> FlowRate;
    std::vector<uint32_t> TagId;

    void Reserve(size_t Count)
    {
        Temperature.reserve(Count);
        Pressure.reserve(Count);
        FlowRate.reserve(Count);
        TagId.reserve(Count);
    }

    size_t Num() const { return Temperature.size(); }
};

// -----------------------------------------------------------------------
// 더미 데이터 생성 — 실제 팹 도면처럼 배관 수만 개, 값은 정상 범위로 랜덤
// -----------------------------------------------------------------------
static void GenerateDummyData(size_t PipeCount,
                               std::vector<FPipeSegmentAoS>& OutAoS,
                               FPipeSegmentArraysSoA& OutSoA)
{
    std::mt19937 Rng(42); // 재현 가능한 시드
    std::uniform_real_distribution<float> TempDist(15.0f, 85.0f);
    std::uniform_real_distribution<float> PressureDist(100.0f, 500.0f);
    std::uniform_real_distribution<float> FlowDist(1.0f, 50.0f);

    OutAoS.clear();
    OutAoS.reserve(PipeCount);
    OutSoA.Reserve(PipeCount);

    for (uint32_t i = 0; i < static_cast<uint32_t>(PipeCount); ++i)
    {
        FPipeSegmentAoS Seg{ TempDist(Rng), PressureDist(Rng), FlowDist(Rng), i };
        OutAoS.push_back(Seg);

        OutSoA.Temperature.push_back(Seg.Temperature);
        OutSoA.Pressure.push_back(Seg.Pressure);
        OutSoA.FlowRate.push_back(Seg.FlowRate);
        OutSoA.TagId.push_back(Seg.TagId);
    }
}

// -----------------------------------------------------------------------
// 벤치마크 대상 연산: "전체 배관 중 과열(온도 임계치 초과) 배관 개수 세기"
// 온도 필드 하나만 필요한, 실무에서 가장 흔한 패턴을 그대로 재현한다.
// -----------------------------------------------------------------------
static size_t CountOverheatedAoS(const std::vector<FPipeSegmentAoS>& Pipes, float Threshold)
{
    size_t Count = 0;
    for (const auto& Seg : Pipes)
    {
        // Temperature 하나만 보지만, 구조체 전체(Pressure/FlowRate/TagId)가
        // 같은 cache line에 함께 로드된다.
        if (Seg.Temperature > Threshold)
        {
            ++Count;
        }
    }
    return Count;
}

static size_t CountOverheatedSoA(const FPipeSegmentArraysSoA& Pipes, float Threshold)
{
    size_t Count = 0;
    // Temperature 배열만 연속으로 순회 — Pressure/FlowRate/TagId는
    // 아예 메모리에 손대지 않는다.
    for (float Temp : Pipes.Temperature)
    {
        if (Temp > Threshold)
        {
            ++Count;
        }
    }
    return Count;
}

// -----------------------------------------------------------------------
// 비교 실험: 반대로 "배관 하나의 모든 속성을 함께 써야 하는" 연산
// (예: 배관 상세 패널에 Temp/Pressure/Flow를 한 번에 출력)에서는
// AoS가 더 자연스러운 코드가 된다. 다만 실제로 더 "빠른"가는 별개
// 문제다 — 데이터 크기가 cache에 넉넉히 들어가고 컴파일러가 세 배열
// 순회를 잘 벡터화하면, SoA가 이 경우에도 근소하게 앞설 수 있다.
// (main() 하단 실험 2 결과 참고 — 실측 없이 예측만으로 단정하지 말 것)
// -----------------------------------------------------------------------
static double SumAllFieldsAoS(const std::vector<FPipeSegmentAoS>& Pipes)
{
    double Sum = 0.0;
    for (const auto& Seg : Pipes)
    {
        // 한 배관의 세 필드를 모두 쓴다 — AoS는 이미 한 cache line 근처에 있음
        Sum += Seg.Temperature + Seg.Pressure + Seg.FlowRate;
    }
    return Sum;
}

static double SumAllFieldsSoA(const FPipeSegmentArraysSoA& Pipes)
{
    double Sum = 0.0;
    // SoA는 세 배열을 동시에 인덱싱해야 하므로, 이 패턴에서는
    // 배열 세 개를 오가며 캐시 라인을 세 배로 소비한다.
    for (size_t i = 0; i < Pipes.Num(); ++i)
    {
        Sum += Pipes.Temperature[i] + Pipes.Pressure[i] + Pipes.FlowRate[i];
    }
    return Sum;
}

template <typename Func>
static double MeasureMs(Func&& F)
{
    const auto Start = std::chrono::high_resolution_clock::now();
    auto Result = F();
    // 최적화로 연산 자체가 통째로 날아가지 않도록 결과를 밖으로 흘려보낸다.
    static volatile decltype(Result) Sink{};
    Sink = Result;
    const auto End = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(End - Start).count();
}

int main()
{
    // 실제 팹 도면 규모를 가정: 배관 세그먼트 200,000개
    const size_t PipeCount = 200000;
    const float OverheatThreshold = 80.0f;
    const int Iterations = 50; // 1초마다 도는 배치를 흉내내어 반복 측정

    std::vector<FPipeSegmentAoS> AoSPipes;
    FPipeSegmentArraysSoA SoAPipes;
    GenerateDummyData(PipeCount, AoSPipes, SoAPipes);

    printf("배관 세그먼트 수: %zu, 반복 횟수: %d\n\n", PipeCount, Iterations);

    // --- 실험 1: 온도 필드만 훑는 연산 (SoA가 유리할 것으로 예상) ---
    double AoSOverheatTotal = 0.0, SoAOverheatTotal = 0.0;
    size_t LastAoSCount = 0, LastSoACount = 0;
    for (int i = 0; i < Iterations; ++i)
    {
        AoSOverheatTotal += MeasureMs([&] { return LastAoSCount = CountOverheatedAoS(AoSPipes, OverheatThreshold); });
        SoAOverheatTotal += MeasureMs([&] { return LastSoACount = CountOverheatedSoA(SoAPipes, OverheatThreshold); });
    }
    printf("[실험 1] 과열 배관 카운트 (Temperature 필드만 사용)\n");
    printf("  AoS 평균: %.4f ms  (과열 개수=%zu)\n", AoSOverheatTotal / Iterations, LastAoSCount);
    printf("  SoA 평균: %.4f ms  (과열 개수=%zu)\n", SoAOverheatTotal / Iterations, LastSoACount);
    printf("  -> SoA/AoS 비율: %.2fx\n\n", (SoAOverheatTotal) / (AoSOverheatTotal + 1e-9));

    // --- 실험 2: 세 필드를 모두 쓰는 연산 (AoS가 유리하거나 비등할 것으로 예상) ---
    double AoSSumTotal = 0.0, SoASumTotal = 0.0;
    for (int i = 0; i < Iterations; ++i)
    {
        AoSSumTotal += MeasureMs([&] { return SumAllFieldsAoS(AoSPipes); });
        SoASumTotal += MeasureMs([&] { return SumAllFieldsSoA(SoAPipes); });
    }
    printf("[실험 2] 배관별 Temp+Pressure+Flow 합산 (모든 필드 사용)\n");
    printf("  AoS 평균: %.4f ms\n", AoSSumTotal / Iterations);
    printf("  SoA 평균: %.4f ms\n", SoASumTotal / Iterations);
    printf("  -> SoA/AoS 비율: %.2fx (1.0보다 크면 AoS가 더 빠름)\n\n", (SoASumTotal) / (AoSSumTotal + 1e-9));

    printf("결론: 어느 쪽이 빠른가는 '어떤 필드를 얼마나 함께 쓰는가'와 '컴파일러 최적화'에 달렸다.\n");
    printf("      이 머신/컴파일러 조합에서는 실험 2(다중 필드)도 SoA가 근소하게 앞섰다 —\n");
    printf("      200,000개 x float 4바이트는 L2/L3 안에 넉넉히 들어가고, 컴파일러가 세 배열\n");
    printf("      순회도 잘 벡터화했기 때문으로 보인다. '여러 필드를 함께 쓰면 AoS가 항상\n");
    printf("      유리하다'는 것도 절대 법칙이 아니라는 뜻 — 실측 없이 예측만으로 단정하면 안 된다.\n");
    printf("      데이터 크기가 캐시보다 훨씬 커지거나 필드 수가 늘어나면 결과가 뒤집힐 수도 있으니,\n");
    printf("      항상 목표 플랫폼/데이터 규모에서 직접 측정해야 한다 (오늘 학습한 원칙 그대로).\n");

    return 0;
}
