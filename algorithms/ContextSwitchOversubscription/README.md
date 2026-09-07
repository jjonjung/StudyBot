# 반도체 배관 CAD - 배관 간섭 검사와 Context Switch / Oversubscription

오늘 학습한 "context switch와 oversubscription" 개념을 반도체 팹(Fab)
배관 CAD 도면의 간섭 검사(clash detection) 상황에 적용해본 예제입니다.
이전 학습인 "NUMA와 thread affinity"의 연장선이며, 다음 학습
("synchronization wait와 blocking")으로 넘어가기 전에 "thread를 많이
만든다고 병렬 성능이 늘어나는 게 아니다"라는 감각을 코드로 확인하는 것이
목적입니다.

## 상황

반도체 팹 배관 CAD 도면 한 장에는 수만 개의 배관 세그먼트가 있고, 설계
변경 후에는 "새로 옮긴 배관이 기존 배관·구조물과 간섭(clash)하지 않는지"
검사해야 합니다. 배관 하나당 주변 배관들과의 거리를 계산하는 clash
detection은 전형적인 CPU-bound 작업입니다.

도면을 여러 구역(zone)으로 나눠 구역별로 worker thread를 하나씩 만들어
병렬 처리한다고 할 때, 구역 수(=thread 수)를 어떻게 정해야 할까요?
"구역을 잘게 쪼갤수록 빠르다"고 생각하기 쉽지만, 실제로는 hardware
thread 수를 한참 넘어서면 오히려 느려질 수 있습니다.

## Context switch란

OS scheduler는 실행 가능한 여러 thread 중 어떤 thread를 CPU에서 실행할지
결정합니다. 현재 thread가 시간 할당량을 다 쓰거나, 더 높은 우선순위
thread가 준비되거나, I/O·동기화를 기다리게 되면 다른 thread로 실행
주체가 바뀔 수 있습니다.

```
현재 thread 상태 저장 -> 다음 실행 thread 선택 -> 다음 thread 상태 복원 -> 실행 재개
```

비용은 레지스터 저장·복원만이 아닙니다. 다른 thread가 실행되는 동안
기존 thread가 데워둔 cache/TLB가 밀려날 수 있고, 다시 돌아왔을 때 그
데이터가 cache에 없어 메모리에서 다시 채워야 할 수 있습니다.

## Oversubscription — [PipeClashOversubscription.cpp](PipeClashOversubscription.cpp)

CPU가 동시에 처리할 수 있는 hardware thread 수보다 활발히 실행하려는
software thread 수가 훨씬 많은 상태입니다. 예를 들어

```
8개 hardware thread
30개 CPU-bound worker thread
```

라면 30개가 동시에 도는 게 아니라 일부만 돌고 나머지는 대기·교체됩니다.
그래서 **thread 수 증가 ≠ 병렬 성능 증가**입니다.

## 실험 설계

같은 총 작업량(배관 세그먼트 전체에 대한 clash detection)을 thread
수만 바꿔가며 나눠 처리하고 총 소요 시간을 측정합니다.

| 실험 | thread 수 | 의미 |
|---|---|---|
| A | `hardware_concurrency()` | 적정 구독 |
| B | `hardware_concurrency() x 8` | 심한 oversubscription |

두 실험 모두 "처리하는 총 연산량"은 동일합니다 — thread 수가 늘어난
만큼 thread 하나가 처리하는 몫이 줄어듭니다. 이상적인(context switch
비용이 0인) 세계라면 두 실험의 총 소요 시간은 비슷해야 합니다. 실측
차이가 있다면 그것이 곧 "과도한 thread 생성·교체 비용"입니다.

### 실측 결과

이 저장소에서 MSVC `/O2`, 배관 200,000개, 10회 반복 평균으로 측정한
결과는 다음과 같았습니다 (환경에 따라 달라질 수 있는 참고용 수치입니다).

```
이 머신의 hardware_concurrency(): 16

[실험 A] thread 수 = 16   (hardware_concurrency 그대로)
  평균 소요 시간: 3.3715 ms

[실험 B] thread 수 = 128  (hardware_concurrency x 8, 심한 oversubscription)
  평균 소요 시간: 8.3208 ms

-> Oversubscribed/Normal 비율: 2.47x
```

thread를 8배 늘렸더니 총 소요 시간이 **2.47배 느려졌습니다.** 같은 총
연산량을 더 잘게 쪼갰을 뿐인데 오히려 느려진 것은, thread 수가 hardware
thread 수(16)를 훨씬 넘어서면서 OS가 더 자주 context switch를 해야
했고, 그때마다 다른 thread가 데워둔 cache/TLB 때문에 재가열 비용이
쌓였기 때문으로 보입니다. 정확한 배율은 CPU 코어 수·OS 스케줄러·부하
상황에 따라 달라지므로, 이 수치 자체보다 "thread를 늘릴수록 항상
빨라지는 건 아니다"라는 방향성을 확인하는 용도로 삼아야 합니다.

## 실행 방법

```
g++ -O2 -std=c++17 -pthread PipeClashOversubscription.cpp -o bench
./bench
```

Windows에서 MSVC로 빌드할 때는 `/utf-8` 옵션 없이도 됩니다.

```
cl /O2 /std:c++17 /EHsc PipeClashOversubscription.cpp
```

배관 200,000개를 생성하고, 실험 A(적정 thread 수)와 실험 B(8배
oversubscription)를 각각 10회 반복해 평균 시간(ms)과 비율을 출력합니다.

### Windows에서 한글이 깨져 보인다면

이 파일은 UTF-8(BOM 포함)로 저장돼 있습니다. AosVsSoa/PipeThermalScan.cpp와
동일한 이유로, 화면 출력은 `printf` 대신 `PrintUtf8()`/`PrintfUtf8()`
헬퍼를 통해 나갑니다 — `u8"..."` 리터럴로 UTF-8 바이트를 표준 보장대로
확보한 뒤 `WriteConsoleW`로 직접 콘솔에 쓰고, 출력이 파일/파이프로
리다이렉트된 경우에는 UTF-8 바이트를 그대로 내보내는 방식입니다. 자세한
원리는 [PipeClashOversubscription.cpp](PipeClashOversubscription.cpp)
상단 `PrintUtf8()` 주석 및 `AosVsSoa/PipeThermalScan.cpp` 참고.

## UE5 실전 버전 — [PipeClashOversubscription_UE5.h](PipeClashOversubscription_UE5.h)

`std::thread`로 직접 thread를 만드는 대신 UE5 `UE::Tasks::Launch`로
옮긴 버전입니다.

- `FPipeClashWorker_Bad` / `RunClashDetection_ManualThreads` — 구역
  수만큼 `FRunnableThread`를 직접 생성하는 안티패턴. 구역을 잘게 쪼갤수록
  그대로 thread가 늘어나 oversubscription으로 이어집니다.
- `RunClashDetection_Tasks` — `UE::Tasks::Launch`로 task를 등록하는 권장
  패턴. task를 몇 개로 쪼개든 실제 실행은 UE의 worker pool(대략
  hardware thread 수 근처, 엔진이 관리)이 순서대로 처리하므로
  oversubscription이 생기지 않습니다.

Unreal Engine 5.8 Tasks 문서는 기다리는 동안 worker를 막는 구조가
scalability를 제한한다고 설명하며, UE 5.5부터 기존 busy waiting을
oversubscription 메커니즘으로 교체했습니다. wait 구간에서는 standby
thread를 깨워 다른 task를 처리하게 하고, 기간이 끝나면 추가 thread를
다시 park합니다. 중요한 점은 이 메커니즘이 "thread를 무한히 많이
만들자"는 뜻이 아니라, blocking 때문에 worker capacity가 일시적으로
줄었을 때 보완하는 장치라는 것입니다.

파일 하단의 "선택 가이드" 주석에 실무 판단 기준을 정리해두었습니다.

## 결론 요약

- `CPU-bound 작업` → task system과 worker pool 우선
- `I/O 대기·장기 blocking` → 비동기 API 또는 적절한 blocking 구조 검토
- `짧은 작업마다 직접 thread 생성` → 생성·스케줄링 비용 때문에 보통 피함
- 목표 플랫폼에서 Unreal Insights와 벤치마크를 통해 항상 실측 확인

## 참고 자료

- Epic UE 5.8 Tasks System: https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine
- Microsoft Thread Scheduling: https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
- Intel VTune Thread Migration: https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2024-0/os-thread-migration.html

## 발표자료

- [PRESENTATION.md](PRESENTATION.md) — 문제 상황부터 실측 비교, UE5
  적용까지 발표용으로 정리한 문서.
