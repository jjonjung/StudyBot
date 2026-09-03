# 반도체 배관 CAD - 배관 온도 스캔과 AoS vs SoA

오늘 학습한 "데이터 배치와 cache 효율(AoS vs SoA)" 개념을 반도체 팹(Fab)
배관 모니터링 상황에 적용해본 예제입니다. 이전 학습인 "cache miss와
hardware prefetch"의 연장선이며, 다음 학습("working set과 false sharing")
으로 넘어가기 전에 "데이터를 어떻게 배치하느냐가 cache 활용에 영향을 준다"는
감각을 코드로 확인하는 것이 목적입니다.

## 상황

반도체 팹 배관 도면 한 장에는 수만 개의 배관 세그먼트가 있고, 각 세그먼트는
온도(Temperature), 압력(Pressure), 유량(FlowRate) 센서 값을 갖습니다.
실무에서 자주 도는 배치성 연산은 이런 식입니다.

- 1초마다 전체 배관의 **온도**만 훑어서 과열 배관 개수를 대시보드에 표시
- 압력이 임계치를 넘은 배관만 필터링해서 경고

즉 배관 하나의 여러 속성을 "함께" 쓰는 게 아니라, 특정 필드 하나만
수만 개에 걸쳐 "쭉 훑는" 연산이 반복되는 hot path입니다. 이런 패턴에서
데이터를 어떻게 배치하느냐(AoS vs SoA)가 성능에 영향을 줄 수 있습니다.

## AoS — [PipeThermalScan.cpp](PipeThermalScan.cpp)의 `FPipeSegmentAoS`

배관 하나의 Temperature/Pressure/FlowRate/TagId를 구조체 하나로 묶고
배열로 저장합니다.

```
[Temp Pressure Flow Tag][Temp Pressure Flow Tag][Temp Pressure Flow Tag]...
```

배관 하나를 통째로 다룰 때(에디터에서 배관 하나 선택해서 상세 정보 표시)는
직관적입니다. 하지만 온도만 훑을 때도 Pressure/FlowRate/TagId까지 같은
cache line에 함께 로드되어, 정작 이번 연산에 쓰지 않는 데이터가 대역폭을
같이 소비합니다.

## SoA — [PipeThermalScan.cpp](PipeThermalScan.cpp)의 `FPipeSegmentArraysSoA`

필드별로 배열을 분리합니다.

```
Temperature: [T T T T T T T T ...]
Pressure:    [P P P P P P P P ...]
FlowRate:    [F F F F F F F F ...]
```

온도만 순회할 때는 Temperature 배열만 연속으로 접근하므로, 필요 없는
Pressure/FlowRate가 cache line에 끼어들지 않습니다. 대신 배관 하나의
세 필드를 모두 써야 하는 연산(실험 2)에서는 배열 세 개를 각각 인덱싱해야
해서 AoS보다 코드가 번거롭고, 접근 패턴에 따라 오히려 불리해질 수 있습니다.

## 두 가지 실험을 함께 넣은 이유

"SoA가 항상 빠르다"고 말하면 틀립니다. [PipeThermalScan.cpp](PipeThermalScan.cpp)는
일부러 상반된 두 실험을 같이 측정합니다.

| 실험 | 연산 | 이론적으로 기대할 수 있는 경향 |
|---|---|---|
| 1 | 온도 필드만 훑어서 과열 배관 카운트 | SoA가 유리할 가능성 (단일 필드 대량 접근) |
| 2 | 배관별 Temp+Pressure+Flow 합산 | AoS가 비등하거나 유리할 가능성 (여러 필드 동시 접근) |

실제로 이 저장소에서 MSVC `/O2`, 배관 200,000개로 측정한 결과는 다음과
같았습니다 (환경에 따라 달라질 수 있는 참고용 수치입니다).

```
[실험 1] AoS 0.1496 ms vs SoA 0.1366 ms  (SoA/AoS = 0.91x)
[실험 2] AoS 0.1571 ms vs SoA 0.1446 ms  (SoA/AoS = 0.92x)
```

실험 1은 예상대로 SoA가 앞섰지만, 실험 2도 SoA가 근소하게 앞섰습니다.
200,000개 x float 4바이트 데이터가 L2/L3 캐시에 넉넉히 들어가고,
컴파일러가 세 배열 순회도 잘 벡터화했기 때문으로 보입니다. 즉 "여러
필드를 함께 쓰면 AoS가 항상 유리하다"는 것도 절대 법칙이 아닙니다.
데이터 크기가 캐시보다 훨씬 커지거나, 필드 수·랜덤 접근 패턴이 달라지면
결과가 뒤집힐 수 있습니다. 이 코드는 "방향성을 눈으로 확인"하는
용도이며, 결론은 항상 목표 환경/데이터 규모에서 직접 측정해서 내려야
합니다.

## 실행 방법

```
g++ -O2 -std=c++17 PipeThermalScan.cpp -o bench
./bench
```

배관 200,000개를 생성하고, 실험 1(단일 필드 스캔)과 실험 2(다중 필드 합산)를
각각 50회 반복해 평균 시간(ms)과 SoA/AoS 비율을 출력합니다.

## UE5 실전 버전 — [PipeThermalScan_UE5.h](PipeThermalScan_UE5.h)

`std::vector` 대신 UE5 `TArray`로 옮긴 버전입니다. `FPipeRegistryAoS`는
`TArray<FPipeSegment>` 하나로, `FPipeRegistrySoA`는 필드별 `TArray`
네 개로 배관을 관리합니다. MassEntity 프레임워크를 도입하지 않고도
"필드별로 TArray를 분리한다"는 것만으로 직접 만든 미니 SoA를 만들 수
있다는 점을 보여주는 것이 이 파일의 요지입니다.

파일 하단의 "선택 가이드" 주석에 규모별 판단 기준(에디터 툴 수준 →
AoS로 충분 / 매 틱 수만 개 필드 스캔 → SoA 검토 / 수만~수십만 규모
군중 시뮬레이션 수준 → 그때 MassEntity 검토)을 정리해두었습니다.

## 결론 요약

- Mass를 "Mass = SoA"로 단순화하기보다는, 객체 전체를 따라가는 OOP식
  접근에서 필요한 데이터 조각만 묶어 batch 처리하기 좋은 구조로 옮겨간
  것으로 이해하는 편이 정확합니다.
- 적 30명 수준의 복잡한 `ACharacter`라면 일반 Actor 구조로 충분할 수
  있습니다. 수천 개 군중의 위치·속도·LOD 같은 단순 상태를 반복 처리한다면
  데이터 지향 배치 구조(SoA, 나아가 MassEntity)의 이점이 커집니다.
- 다만 SoA/MassEntity 모두 도입 비용(코드 복잡도, 디버깅 난이도, 팀
  숙련도)이 있으므로 "성능이 좋아 보이니까" 바로 도입하지 않고, 목표
  플랫폼에서 Unreal Insights와 벤치마크로 확인한 뒤 결정합니다.
