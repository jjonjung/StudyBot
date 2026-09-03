// PipeThermalScan_UE5.h
//
// PipeThermalScan.cpp(순수 C++)와 동일한 AoS/SoA 비교를 UE5 컨테이너로
// 옮긴 버전. UE5 프로젝트(예: UnrealStudyBot)에서 배관/설비 모니터링
// 대시보드를 만든다고 가정한 실전 코드다.
//
// 핵심 포인트: MassEntity 같은 별도 프레임워크를 도입하지 않아도,
// TArray를 필드별로 분리하는 것만으로 "직접 만든 미니 SoA"를 만들 수 있다.
// 수천~수만 개의 배관을 매 틱 스캔해야 하는데 Actor/ACharacter 단위로
// 만들 필요는 없는 상황(순수 데이터 + 배치 처리)에 맞는 절충안이다.

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------
// AoS 버전 — "배관 하나 = FPipeSegment 하나"를 TArray에 담는다.
// ---------------------------------------------------------------------
//
// 디테일 패널에 배관 하나를 선택해서 Temp/Pressure/Flow를 한 번에
// 보여줘야 하는 에디터 툴이라면 이 구조가 훨씬 자연스럽다.
USTRUCT()
struct FPipeSegment
{
    GENERATED_BODY()

    UPROPERTY()
    float Temperature = 0.0f;

    UPROPERTY()
    float Pressure = 0.0f;

    UPROPERTY()
    float FlowRate = 0.0f;

    UPROPERTY()
    uint32 TagId = 0;
};

/**
 * AoS 컨테이너. TArray<FPipeSegment> 하나로 배관 전체를 관리한다.
 * ContainerSelection/README.md 기준으로는 "TArray: 정답 데이터, 순서
 * 보존" 케이스에 해당 — 여기서는 순서 자체보다 "필드가 함께 붙어있다"는
 * 점이 핵심이다.
 */
class FPipeRegistryAoS
{
public:
    void AddPipe(const FPipeSegment& Segment)
    {
        Pipes.Add(Segment);
    }

    /**
     * 매 틱(혹은 1초 주기 타이머)마다 도는 과열 배관 카운트.
     * Temperature 하나만 쓰지만, FPipeSegment 전체가 cache line에
     * 함께 올라온다 — PipeThermalScan.cpp의 CountOverheatedAoS와 동일 패턴.
     */
    int32 CountOverheated(float Threshold) const
    {
        int32 Count = 0;
        for (const FPipeSegment& Seg : Pipes)
        {
            if (Seg.Temperature > Threshold)
            {
                ++Count;
            }
        }
        return Count;
    }

    /** 배관 상세 패널용 — 한 배관의 모든 필드를 함께 조회. AoS가 자연스러운 경우. */
    const FPipeSegment* GetPipe(int32 Index) const
    {
        return Pipes.IsValidIndex(Index) ? &Pipes[Index] : nullptr;
    }

    int32 Num() const { return Pipes.Num(); }

private:
    UPROPERTY()
    TArray<FPipeSegment> Pipes;
};

// ---------------------------------------------------------------------
// SoA 버전 — 필드별로 TArray를 분리해서 나란히 둔다.
// ---------------------------------------------------------------------
//
// MassEntity의 Fragment 배열이 하는 일을 아주 단순화하면 이 모양이다:
// "같은 인덱스는 같은 엔티티"라는 규약만 지키면, 엔진 프레임워크 없이도
// 필드 단위로 조밀하게 데이터를 배치할 수 있다.
class FPipeRegistrySoA
{
public:
    void AddPipe(float Temperature, float Pressure, float FlowRate, uint32 TagId)
    {
        // 네 배열의 길이를 항상 동일하게 유지하는 것이 이 구조의 불변식(invariant).
        Temperatures.Add(Temperature);
        Pressures.Add(Pressure);
        FlowRates.Add(FlowRate);
        TagIds.Add(TagId);
    }

    /**
     * Temperatures 배열만 연속으로 순회 — Pressures/FlowRates/TagIds는
     * 이 연산 동안 아예 메모리에 손대지 않는다.
     * 배관 수가 수만 개 규모이고 이 스캔이 매 틱 도는 hot path라면,
     * AoS 대비 체감 가능한 차이가 날 수 있다 (목표 플랫폼에서 실측 필요).
     */
    int32 CountOverheated(float Threshold) const
    {
        int32 Count = 0;
        for (float Temp : Temperatures)
        {
            if (Temp > Threshold)
            {
                ++Count;
            }
        }
        return Count;
    }

    /**
     * 배관 상세 조회는 인덱스로 네 배열을 각각 접근해야 한다 —
     * AoS의 GetPipe보다 번거롭다는 점이 SoA의 트레이드오프다.
     */
    FPipeSegment GetPipe(int32 Index) const
    {
        FPipeSegment Seg;
        if (Temperatures.IsValidIndex(Index))
        {
            Seg.Temperature = Temperatures[Index];
            Seg.Pressure = Pressures[Index];
            Seg.FlowRate = FlowRates[Index];
            Seg.TagId = TagIds[Index];
        }
        return Seg;
    }

    int32 Num() const { return Temperatures.Num(); }

private:
    UPROPERTY()
    TArray<float> Temperatures;

    UPROPERTY()
    TArray<float> Pressures;

    UPROPERTY()
    TArray<float> FlowRates;

    UPROPERTY()
    TArray<uint32> TagIds;
};

// ---------------------------------------------------------------------
// 선택 가이드 (오늘 학습 결론을 이 도메인에 맞게 정리)
// ---------------------------------------------------------------------
//
// - 배관 수가 적고(수십~수백), 에디터 툴처럼 "배관 하나씩 자세히 다루는"
//   작업이 중심이면 FPipeRegistryAoS로 충분하다. 코드가 단순하고
//   디버깅(디테일 패널에서 구조체 하나 보기)도 쉽다.
//
// - 배관 수가 수만 개 규모이고, 매 틱/매초 "특정 필드 하나만" 대량으로
//   스캔하는 배치 연산(과열 감지, 압력 임계치 필터링, 평균 계산)이
//   반복적으로 도는 hot path라면 FPipeRegistrySoA 쪽이 유리할 가능성이
//   커진다 — 다만 "가능성"이지 항상 그런 것은 아니므로 Unreal Insights로
//   실측해서 결정한다.
//
// - 이보다 더 큰 규모(수만~수십만, 군중 시뮬레이션 수준)이고 배관 상태
//   전이(정상->경고->위험)나 LOD까지 엔진 스케줄러가 관리해주길 원한다면
//   그때 MassEntity 도입을 검토한다. 다만 MassEntity 자체의 학습·디버깅
//   비용이 있으므로, 이 정도 규모가 아니라면 위의 수동 SoA로 충분하다.
