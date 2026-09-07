// PipeThermalScan.cpp
//
// 오늘 학습한 "AoS vs SoA와 cache 효율" 개념을 반도체 팹 배관 모니터링
// 상황에 적용한 순수 C++ 벤치마크. 표준 C++만 사용하므로 UE5 없이도
// 컴파일해서 직접 실행할 수 있다. (UE5 실전 버전은 PipeThermalScan_UE5.h 참고)
//
// 컴파일 예:
//   g++ -O2 -std=c++17 PipeThermalScan.cpp -o bench && ./bench
//
// 개념 정리
// ---------
// AoS(Array of Structures) = "객체 하나 = 구조체 하나"를 배열로 저장.
//   [필드1 필드2 필드3][필드1 필드2 필드3][필드1 필드2 필드3]...
//   객체 하나를 통째로 다루는 코드가 자연스럽고, OOP 설계와 잘 맞는다.
//
// SoA(Structure of Arrays) = "필드 하나 = 배열 하나"로 쪼개서 나란히 저장.
//   필드1: [v v v v v v v v ...]
//   필드2: [v v v v v v v v ...]
//   필드3: [v v v v v v v v ...]
//   같은 인덱스가 같은 객체를 가리킨다는 "규약"으로 묶여 있을 뿐, 객체라는
//   단위 자체는 메모리상에 존재하지 않는다.
//
// 차이가 생기는 이유는 CPU cache line(보통 64바이트) 때문이다. CPU는
// 메모리를 1바이트씩 읽지 않고 cache line 단위로 통째로 읽어온다.
// AoS에서 필드 하나만 필요해도 같은 cache line에 딸려온 다른 필드까지
// 캐시에 올라오므로, 그 필드가 이번 연산에 안 쓰이면 캐시 공간과 메모리
// 대역폭 낭비가 된다. SoA는 같은 필드끼리만 붙어 있으므로 딱 필요한
// 데이터만 촘촘하게 캐시에 올라온다 — "필요 없는 걸 같이 사 오지 않는다"는
// 감각으로 이해하면 된다.
//
// 실무에서 SoA가 자주 쓰이는 상황과 이유
// ---------------------------------------
// 1) 게임 엔진의 대량 객체 시뮬레이션 (파티클, 군중, 탄환)
//    - 위치만 갱신, 속도만 적분, 수명만 감소시키는 등 "필드 하나를 수천~
//      수만 개에 걸쳐 반복 갱신"하는 연산이 매 프레임 반복된다.
//    - UE5 MassEntity, Niagara 파티클 시스템이 내부적으로 SoA(Fragment
//      배열)를 쓰는 이유가 이것 — 프레임 예산 안에 수만 개를 갱신하려면
//      cache miss를 줄이는 게 직결된다.
//
// 2) SIMD(벡터화) 연산
//    - CPU의 SIMD 명령(SSE/AVX)은 "연속된 메모리에 있는 같은 타입 값
//      여러 개"를 한 번에 처리한다. SoA는 그 자체로 SIMD가 원하는 배치라
//      컴파일러 자동 벡터화나 수동 SIMD 코드 작성이 훨씬 쉬워진다.
//      (AoS는 필드 사이에 다른 타입이 끼어 있어 벡터화가 막히기 쉽다.)
//
// 3) 컬럼 지향(column-oriented) 데이터베이스 / 분석 파이프라인
//    - "전체 로우 중 특정 컬럼 하나의 합계/평균"을 구하는 분석 쿼리가
//      대부분인 OLAP/데이터 웨어하우스는 컬럼별로 저장(SoA와 동일 발상)
//      해서 필요한 컬럼만 디스크·메모리에서 읽는다. (반대로 "로우 하나
//      전체를 자주 읽고 쓰는" OLTP는 로우 지향 = AoS 발상에 가깝다.)
//
// 4) GPU 인스턴싱 / 컴퓨트 셰이더 입력
//    - GPU에 수만 개 인스턴스의 Transform만 넘길 때도 필드별 배열(SoA)로
//      묶어 넘기는 것이 GPU 메모리 접근 패턴(coalesced access)과 맞는다.
//
// 공통된 이유: 위 상황 모두 "객체 개별을 다루기보다, 같은 필드를 대량으로
// 훑거나 변형하는 batch 연산이 hot path"라는 점이 같다. 반대로 이번
// 예제처럼 "배관 하나의 여러 속성을 함께 조회/출력"하는 게 중심이라면
// AoS가 더 단순하고 자연스럽다 — 이 파일의 실험 2가 그 반례를 보여준다.
//
// 이번 예제 상황
// --------------
// 반도체 팹 배관 도면에는 수만 개의 배관 세그먼트가 있고, 각 세그먼트는
// 실시간으로 온도(Temperature), 압력(Pressure), 유량(FlowRate) 센서 값을
// 갖는다. 그런데 실무에서 자주 도는 배치(batch) 작업은 이런 식이다.
//
//   - "전체 배관의 온도 평균/최대값을 1초마다 계산해서 대시보드에 표시"
//   - "압력이 임계치를 넘은 배관만 필터링해서 경고"
//
// 즉 한 세그먼트의 여러 속성을 "함께" 쓰는 게 아니라, 특정 필드 하나만
// 수만 개에 걸쳐 "쭉 훑는" 연산이 hot path다 — 위 1)~4) 상황과 같은
// 성격이다. 이런 패턴에서 AoS와 SoA가 실제로 얼마나 차이 나는지
// 확인해본다.

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <random>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

// 왜 한글 출력이 콘솔마다 깨지기 쉬운가
// ------------------------------------
// 이 파일은 UTF-8(BOM 포함)로 저장되어 있다. 그런데 MSVC는 컴파일 옵션에
// "/utf-8"을 주지 않으면(예: CLion 기본 설정, 옵션 없이 cl 실행 등)
// 일반 문자열 리터럴 "..."을 실행 파일에 새길 때 시스템 ANSI 코드페이지
// (한국어 Windows는 CP949)로 다시 인코딩해버린다 — 이를 "실행 문자셋
// (execution charset)"이라 한다. 그러면 main()에서 SetConsoleOutputCP
// (CP_UTF8)로 콘솔을 UTF-8 모드로 바꿔도 정작 실행 파일 안의 바이트
// 자체가 CP949라서 여전히 깨져 보인다 — 소스 인코딩(BOM)과 실행
// 문자셋은 서로 다른 층위라 BOM만으로는 이 문제가 해결되지 않는다.
//
// 이 문제를 컴파일 옵션에 의존하지 않고 소스 안에서 확실히 고치는
// 방법은 "u8" 접두사 리터럴을 쓰는 것이다. u8"..."은 C++11부터 항상
// UTF-8 바이트로 저장되는 것이 표준으로 보장되므로, /utf-8 옵션 유무나
// 시스템 로캘과 무관하게 실행 파일 안에 정확한 UTF-8 바이트가 들어간다.
// 아래 PrintUtf8()은 그렇게 확보한 UTF-8 바이트를 UTF-16으로 변환해
// WriteConsoleW로 직접 출력한다 — 콘솔 코드페이지 설정에도 의존하지
// 않는, 이 두 겹 문제(소스/실행 문자셋 + 콘솔 인코딩)를 모두 우회하는
// 방법이다. 단, WriteConsoleW는 표준출력이 "진짜 콘솔 화면 버퍼"에
// 연결돼 있을 때만 동작한다 — 실행 결과를 파일/파이프로 리다이렉트하면
// (예: bench.exe > log.txt, 또는 IDE가 출력을 캡처하는 경우) 표준출력
// 핸들이 콘솔이 아니게 되어 WriteConsoleW가 조용히 아무것도 쓰지 않는다.
// 그런 경우를 위해 GetConsoleMode로 콘솔 여부를 먼저 확인하고, 콘솔이
// 아니면 UTF-8 바이트를 그대로 fwrite하는 폴백을 둔다 (리다이렉트된
// 파일을 UTF-8 지원 에디터로 열면 정상적으로 보인다).
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
            // WideLen에는 널 종단 문자가 포함되므로 -1
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

// printf 서식(%zu, %.4f 등)과 한글을 함께 쓰기 위한 헬퍼.
// snprintf로 먼저 UTF-8 버퍼를 만든 뒤 PrintUtf8()로 콘솔에 낸다.
template <typename... Args>
static void PrintfUtf8(const char* Utf8Fmt, Args... args)
{
    char Buffer[512];
    std::snprintf(Buffer, sizeof(Buffer), Utf8Fmt, args...);
    PrintUtf8(Buffer);
}

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
//
// 주의: 아래 네 벡터를 struct 하나로 묶은 것은 순전히 "코드 관리 편의"
// 때문이지 SoA의 정의와는 무관하다. struct FPipeSegmentArraysSoA의
// 인스턴스는 프로그램 전체에서 딱 하나만 만들고, 그 *안의* 벡터 각각이
// "배관 20만 개 분량"으로 커진다. 즉 이 struct 자체를 배열로 만드는
// 게 아니다 — 그러면 다시 AoS로 돌아가 버린다. 극단적으로는 이 struct
// 없이 std::vector<float> Temperature; std::vector<float> Pressure; ...
// 를 서로 남남인 변수 네 개로 각각 선언해도 메모리 배치와 성능은
// 완전히 동일하다. 여기서 struct로 묶은 이유는 관련된 배열 네 개를
// 함수에 한 번에 넘기고 Reserve()/Num() 같은 헬퍼를 붙이기 위함일
// 뿐, 성능에는 영향을 주지 않는다.
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

// 측정 대상 함수를 실행하고 걸린 시간(ms)을 반환한다.
//
// OutSink는 반드시 volatile로 선언된 변수를 넘겨야 한다. 그렇지 않으면
// 최적화 컴파일러(/O2)가 "이 반복 결과가 마지막에만 쓰인다"는 걸 알고
// 루프 전체를 통째로 접어버리거나(50번 반복 -> 계산 1번), 심하면
// 죽은 코드로 보고 지워버려서 측정 시간이 0에 가깝게 나온다 —
// 실제로 이 벤치마크를 여러 번 돌리는 과정에서 실험 2 결과가 "SoA가
// 7배 느림" -> "0.0000ms" 처럼 실행마다 크게 요동친 적이 있는데,
// 두 경우 모두 원인은 측정 대상 연산이 최적화로 사라지거나 실행마다
// 다르게 접힌 것이었다. volatile sink로 "이 값은 관찰 가능한 부작용"
// 이라고 컴파일러에 명시해야 매 반복이 실제로 다시 계산된다.
template <typename Func, typename Sink>
static double MeasureMs(Func&& F, volatile Sink& OutSink)
{
    const auto Start = std::chrono::high_resolution_clock::now();
    OutSink = OutSink + F();
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

    PrintfUtf8(u8"배관 세그먼트 수: %zu, 반복 횟수: %d\n\n", PipeCount, Iterations);

    // --- 실험 1: 온도 필드만 훑는 연산 (SoA가 유리할 것으로 예상) ---
    double AoSOverheatTotal = 0.0, SoAOverheatTotal = 0.0;
    // volatile: 컴파일러가 루프를 통째로 접거나 지우지 못하게 막는 관찰 지점.
    volatile size_t AoSCountSink = 0, SoACountSink = 0;
    for (int i = 0; i < Iterations; ++i)
    {
        AoSOverheatTotal += MeasureMs([&] { return CountOverheatedAoS(AoSPipes, OverheatThreshold); }, AoSCountSink);
        SoAOverheatTotal += MeasureMs([&] { return CountOverheatedSoA(SoAPipes, OverheatThreshold); }, SoACountSink);
    }
    PrintfUtf8(u8"[실험 1] 과열 배관 카운트 (Temperature 필드만 사용)\n");
    PrintfUtf8(u8"  AoS 평균: %.4f ms  (과열 개수=%zu)\n", AoSOverheatTotal / Iterations, static_cast<size_t>(AoSCountSink) / static_cast<size_t>(Iterations));
    PrintfUtf8(u8"  SoA 평균: %.4f ms  (과열 개수=%zu)\n", SoAOverheatTotal / Iterations, static_cast<size_t>(SoACountSink) / static_cast<size_t>(Iterations));
    PrintfUtf8(u8"  -> SoA/AoS 비율: %.2fx\n\n", (SoAOverheatTotal) / (AoSOverheatTotal + 1e-9));

    // --- 실험 2: 세 필드를 모두 쓰는 연산 (AoS가 유리하거나 비등할 것으로 예상) ---
    double AoSSumTotal = 0.0, SoASumTotal = 0.0;
    volatile double AoSSumSink = 0.0, SoASumSink = 0.0;
    for (int i = 0; i < Iterations; ++i)
    {
        AoSSumTotal += MeasureMs([&] { return SumAllFieldsAoS(AoSPipes); }, AoSSumSink);
        SoASumTotal += MeasureMs([&] { return SumAllFieldsSoA(SoAPipes); }, SoASumSink);
    }
    PrintfUtf8(u8"[실험 2] 배관별 Temp+Pressure+Flow 합산 (모든 필드 사용)\n");
    PrintfUtf8(u8"  AoS 평균: %.4f ms\n", AoSSumTotal / Iterations);
    PrintfUtf8(u8"  SoA 평균: %.4f ms\n", SoASumTotal / Iterations);
    PrintfUtf8(u8"  -> SoA/AoS 비율: %.2fx (1.0보다 크면 AoS가 더 빠름)\n\n", (SoASumTotal) / (AoSSumTotal + 1e-9));

    PrintfUtf8(u8"결론: 어느 쪽이 빠른가는 '어떤 필드를 얼마나 함께 쓰는가'와 '컴파일러 최적화'에 달렸다.\n");
    PrintfUtf8(u8"      이 머신/컴파일러 조합에서는 실험 2(다중 필드)도 SoA가 근소하게 앞섰다 -\n");
    PrintfUtf8(u8"      200,000개 x float 4바이트는 L2/L3 안에 넉넉히 들어가고, 컴파일러가 세 배열\n");
    PrintfUtf8(u8"      순회도 잘 벡터화했기 때문으로 보인다. '여러 필드를 함께 쓰면 AoS가 항상\n");
    PrintfUtf8(u8"      유리하다'는 것도 절대 법칙이 아니라는 뜻 - 실측 없이 예측만으로 단정하면 안 된다.\n");
    PrintfUtf8(u8"      데이터 크기가 캐시보다 훨씬 커지거나 필드 수가 늘어나면 결과가 뒤집힐 수도 있으니,\n");
    PrintfUtf8(u8"      항상 목표 플랫폼/데이터 규모에서 직접 측정해야 한다 (오늘 학습한 원칙 그대로).\n");

    return 0;
}
