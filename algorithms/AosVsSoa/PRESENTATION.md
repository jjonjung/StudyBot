# 발표자료 — 배관 온도 스캔, AoS vs SoA는 왜 다른가

오늘 학습한 "데이터 배치와 cache 효율(AoS vs SoA)" 개념을, 반도체 팹
배관 모니터링 상황으로 재구성하고 실측 수치까지 뽑아 정리했다. 발표 흐름:
**개념 → 실무 사용처 → 문제 상황 → AoS → SoA → 실측 비교(반전 포함)
→ UE5 실전 적용 → 결론**.

---

## 0. 개념 — AoS와 SoA란

### 비유로 먼저

학생 100명의 이름·키·몸무게를 기록한다고 하면:

- **AoS**: 학생 한 명당 카드 한 장에 이름·키·몸무게를 같이 적고, 카드를
  100장 쌓아둔다. 학생 한 명을 통째로 볼 때 카드 한 장만 뽑으면 된다.
- **SoA**: 이름만 적은 명부, 키만 적은 명부, 몸무게만 적은 명부를 따로
  만든다. 같은 줄 번호가 같은 학생을 가리킨다는 "약속"으로만 묶여 있다.
  키 평균만 구하고 싶으면 키 명부 한 권만 펼치면 된다.

**AoS (Array of Structures)** — "객체 하나 = 구조체 하나"를 배열로 저장.

```
[필드1 필드2 필드3][필드1 필드2 필드3][필드1 필드2 필드3]...
```

객체 하나를 통째로 다루는 코드가 자연스럽고, 흔히 쓰는 OOP 설계
(`class Enemy { Position; Velocity; HP; }`, `TArray<Enemy>`)와 잘 맞는다.

**SoA (Structure of Arrays)** — "필드 하나 = 배열 하나"로 쪼개서
나란히 저장.

```
필드1: [v v v v v v v v ...]
필드2: [v v v v v v v v ...]
필드3: [v v v v v v v v ...]
```

같은 인덱스가 같은 객체를 가리킨다는 "규약"으로만 묶여 있고, 객체라는
단위 자체는 메모리상에 실재하지 않는다.

### 왜 차이가 생기는가 — cache line

CPU는 메모리를 1바이트씩 읽지 않고 **cache line 단위(보통 64바이트)**로
통째로 읽어온다.

- AoS에서 필드 하나만 필요해도, 같은 cache line에 함께 실린 다른
  필드까지 캐시에 딸려 올라온다. 그 필드를 이번 연산에서 안 쓰면
  캐시 공간과 메모리 대역폭을 낭비한 것이 된다.
- SoA는 같은 필드끼리만 붙어 있으므로, 필요한 데이터만 촘촘하게
  캐시에 올라온다.

"장 볼 때 필요한 것만 사 오는가, 세트로 묶어 파는 걸 사서 안 쓰는
것도 같이 들고 오는가"의 차이로 비유하면 감이 잡힌다.

### 그래서 언제 뭘 쓰나 — 한눈에 요약

| 상황 | 유리한 쪽 | 이유 |
|---|---|---|
| 객체 하나를 통째로 다룸 (디테일 패널에서 학생/적 1명 정보 표시) | AoS | 카드 한 장(구조체 하나)만 뽑으면 끝 — 코드도 자연스럽고 단순 |
| 필드 하나만 수천~수만 개 걸쳐 훑음 (전체 HP 합계, 파티클 위치 갱신) | SoA | 그 필드만 모은 명부 한 권만 펼치면 됨 — 캐시 낭비가 줄어듦 |

다만 이 표는 "이론적으로 기대할 수 있는 경향"이지 절대 법칙이 아니다.
4장의 실측 결과가 그 이유를 보여준다.

---

## 0-1. 실무에서 SoA가 자주 쓰이는 상황과 이유

| 상황 | 왜 SoA가 유리한가 |
|---|---|
| **게임 대량 객체 시뮬레이션**(파티클, 군중, 탄환) | 위치만 갱신·속도만 적분·수명만 감소시키는 등 "필드 하나를 수천~수만 개에 걸쳐 반복 갱신"하는 연산이 매 프레임 반복된다. UE5 **MassEntity의 Fragment**, **Niagara 파티클**이 내부적으로 이 방식을 쓴다. |
| **SIMD(벡터화) 연산** | SSE/AVX 같은 SIMD 명령은 "연속 메모리에 있는 같은 타입 값 여러 개"를 한 번에 처리한다. SoA는 그 자체로 SIMD가 원하는 배치라 컴파일러 자동 벡터화나 수동 SIMD 작성이 쉬워진다. AoS는 필드 사이에 다른 타입이 끼어 있어 벡터화가 막히기 쉽다. |
| **컬럼 지향(OLAP) 데이터베이스 / 분석 파이프라인** | "전체 로우 중 특정 컬럼 하나의 합계·평균"을 구하는 분석 쿼리가 대부분이라, 컬럼별로 저장(SoA와 동일 발상)해서 필요한 컬럼만 디스크·메모리에서 읽는다. |
| **GPU 인스턴싱 / 컴퓨트 셰이더 입력** | 수만 개 인스턴스의 Transform을 GPU에 넘길 때도 필드별 배열(SoA)로 묶는 것이 GPU의 coalesced memory access 패턴과 맞는다. |

공통점: **객체 개별을 다루기보다, 같은 필드를 대량으로 훑거나 변형하는
batch 연산이 hot path**라는 것. 반대로 "객체 하나의 여러 속성을 함께
조회·출력"하는 게 중심이면 AoS가 더 단순하고 자연스럽다 — 이 발표
4장의 실험 2가 그 반례를 실측으로 보여준다.

---

## 1. 문제 상황

반도체 팹 배관 도면 한 장에는 수만 개의 배관 세그먼트가 있고, 각
세그먼트는 온도(Temperature)·압력(Pressure)·유량(FlowRate) 센서 값을
갖는다.

실무에서 자주 도는 배치(batch) 연산은 이런 식이다.

- 1초마다 전체 배관의 **온도만** 훑어서 과열 배관 개수를 대시보드에 표시
- 압력이 임계치를 넘은 배관만 필터링해서 경고

즉 배관 하나의 여러 속성을 "함께" 쓰는 게 아니라, **특정 필드 하나만
수만 개에 걸쳐 쭉 훑는** 연산이 반복되는 hot path다. 이런 접근 패턴에서
"데이터를 메모리에 어떻게 배치하느냐"가 실제로 체감 가능한 차이를
만들 수 있는지 확인해본다.

---

## 2. AoS — 배관 하나를 구조체로 묶는다

가장 직관적으로 짜는 방식: 배관 하나의 속성을 구조체 하나에 담고
배열로 저장한다.

```cpp
// PipeThermalScan.cpp — FPipeSegmentAoS
struct FPipeSegmentAoS
{
    float Temperature;
    float Pressure;
    float FlowRate;
    uint32_t TagId;
};
std::vector<FPipeSegmentAoS> Pipes;
```

메모리 배치(개념도):

```
[Temp Pressure Flow Tag][Temp Pressure Flow Tag][Temp Pressure Flow Tag]...
```

배관 하나를 통째로 다룰 때(에디터에서 배관 하나 선택해 상세 정보 표시)는
직관적이고 자연스럽다. 하지만 **온도만 훑는 연산에서도** Pressure·
FlowRate·TagId까지 같은 cache line에 함께 실려 온다 — 이번 연산에
쓰지 않는 데이터가 대역폭을 같이 갉아먹는 셈이다.

```cpp
// PipeThermalScan.cpp — CountOverheatedAoS()
for (const auto& Seg : Pipes)
{
    if (Seg.Temperature > Threshold) ++Count; // Temp만 쓰지만 구조체 전체가 로드됨
}
```

---

## 3. SoA — 속성별로 배열을 분리한다

```cpp
// PipeThermalScan.cpp — FPipeSegmentArraysSoA
struct FPipeSegmentArraysSoA
{
    std::vector<float> Temperature;
    std::vector<float> Pressure;
    std::vector<float> FlowRate;
    std::vector<uint32_t> TagId;
};
```

메모리 배치(개념도):

```
Temperature: [T T T T T T T T ...]
Pressure:    [P P P P P P P P ...]
FlowRate:    [F F F F F F F F ...]
```

온도만 순회할 때는 Temperature 배열만 연속 접근하므로, 필요 없는
Pressure·FlowRate가 cache line에 끼어들지 않는다.

```cpp
// PipeThermalScan.cpp — CountOverheatedSoA()
for (float Temp : Pipes.Temperature)
{
    if (Temp > Threshold) ++Count; // 이 연산에 필요한 데이터만 메모리에서 접근
}
```

대신 배관 하나의 세 필드를 모두 써야 하는 연산에서는 배열 세 개를
각각 인덱싱해야 해서 코드가 AoS보다 번거로워진다.

---

## 4. 실측 비교 — 두 가지 상반된 연산을 같은 데이터로 측정

"SoA가 항상 빠르다"는 말은 틀리기 쉽다. 그래서 일부러 상반된 두
실험을 같은 데이터(배관 200,000개, MSVC `/O2`, 50회 반복 평균)로
같이 측정했다.

| 실험 | 연산 | 이론상 기대 경향 |
|---|---|---|
| 1 | 온도 필드만 훑어 과열 배관 카운트 | SoA 유리 (단일 필드 대량 접근) |
| 2 | 배관별 Temp+Pressure+Flow 합산 | AoS가 비등하거나 유리 (여러 필드 동시 접근) |

### 실측 결과

```
[실험 1] AoS 0.1496 ms  vs  SoA 0.1366 ms   -> SoA/AoS = 0.91x
[실험 2] AoS 0.1571 ms  vs  SoA 0.1446 ms   -> SoA/AoS = 0.92x
```

실험 1은 예상대로 SoA가 앞섰다. **그런데 실험 2도 SoA가 근소하게
앞섰다** — "여러 필드를 함께 쓰면 AoS가 유리할 것"이라는 이론적
기대와 실측이 어긋난 지점이다.

### 왜 이렇게 나왔는가

- 배관 200,000개 x float 4바이트 데이터가 L2/L3 캐시에 넉넉히 들어가는
  규모라, AoS의 "불필요한 필드까지 로드되는" 불이익이 크게 드러나지
  않았다.
- 컴파일러(`/O2`)가 SoA의 세 배열 순회(실험 2)도 잘 벡터화해서, 배열을
  세 번 인덱싱하는 코드상의 번거로움이 실행 속도 손해로 이어지지
  않았다.

> 재현: `PipeThermalScan.cpp`를 컴파일해 실행하면 동일한 방식으로
> 직접 확인할 수 있다. (`g++ -O2 -std=c++17 PipeThermalScan.cpp -o bench`)

**이 반전이 발표의 핵심 포인트다.** "이론상 이래야 한다"와 "실측
결과"가 다를 수 있다는 것 자체가, 오늘 학습한 원칙 — *SoA가 항상
빠르다고 말하면 안 되고, 데이터 크기·접근 패턴·컴파일러 최적화에
따라 달라지므로 목표 플랫폼에서 직접 측정해야 한다* — 을 코드로
증명한 사례다.

---

## 5. C++ → UE5, 두 버전의 관계

| 파일 | 역할 |
|---|---|
| [PipeThermalScan.cpp](PipeThermalScan.cpp) | **벤치마크** — 표준 C++로 AoS/SoA를 각각 구현하고 실측 시간을 직접 측정 |
| [PipeThermalScan_UE5.h](PipeThermalScan_UE5.h) | **실전 적용** — `std::vector`를 UE5 `TArray`로 옮긴 버전. `FPipeRegistryAoS`(TArray 하나) vs `FPipeRegistrySoA`(필드별 TArray 네 개) |

```cpp
// PipeThermalScan_UE5.h — SoA는 이렇게 "필드별 TArray"로 구성한다
TArray<float> Temperatures;
TArray<float> Pressures;
TArray<float> FlowRates;
TArray<uint32> TagIds;
```

**핵심 메시지**: MassEntity 같은 별도 프레임워크 없이도, "필드별로
TArray를 나란히 둔다"는 규칙만 지키면 직접 만든 미니 SoA를 만들 수
있다. MassEntity의 Fragment 배열이 하는 일을 단순화하면 이 모양이다.

UE5/게임 도메인에서 이 패턴이 실제로 쓰이는 대표 사례: Niagara
파티클(위치·속도·수명을 파티클 개수만큼 배열로 갱신), MassEntity의
Fragment(`FTransformFragment`, `FHealthFragment` 등을 엔티티별 구조체가
아니라 필드별 배열=Chunk로 저장), 애니메이션 본 트랜스폼 배치 갱신,
인스턴스드 스태틱 메시의 PerInstance 데이터 업로드. 전부 "엔티티를
하나씩 순회하기보다 같은 필드를 수천~수만 개에 걸쳐 한 번에 갱신"하는
것이 매 틱 반복되는 hot path라는 공통점이 있다.

파일 하단에 규모별 판단 기준을 정리해두었다.

| 상황 | 권장 |
|---|---|
| 배관 수십~수백, 에디터 툴에서 배관 하나씩 상세 조회 중심 | AoS로 충분 |
| 배관 수만 개, 매 틱/매초 특정 필드 하나만 대량 스캔 | SoA 검토 (실측 필요) |
| 수만~수십만 규모, 상태 전이·LOD까지 엔진이 관리해주길 원함 | 그때 MassEntity 검토 |

---

## 6. 발표 요약 한 줄

> **"SoA가 이론적으로 유리해 보인다"와 "실제로 더 빠르다"는 다른
> 문제다 — 이번 벤치마크에서는 여러 필드를 함께 쓰는 연산조차 SoA가
> 근소하게 앞섰고, 그 이유는 데이터가 캐시 크기 안에 들어가고
> 컴파일러가 잘 벡터화했기 때문이었다. 결론은 항상 목표 환경에서
> 직접 측정해서 내려야 한다.**

---

## 부록 — 참고: Mass ≠ SoA 단순 등식

- Mass를 "Mass = SoA"로 단순화하기보다는, 객체 전체를 따라가는 OOP식
  접근에서 필요한 데이터 조각만 묶어 batch 처리하기 좋은 구조로
  옮겨간 것으로 이해하는 편이 정확하다.
- 적 30명 수준의 복잡한 `ACharacter`라면 일반 Actor 구조로 충분할 수
  있다. 수천 개 군중의 위치·속도·LOD 같은 단순 상태를 반복 처리한다면
  데이터 지향 배치 구조(SoA, 나아가 MassEntity)의 이점이 커진다.
- 다만 SoA/MassEntity 모두 도입 비용(코드 복잡도, 디버깅 난이도, 팀
  숙련도)이 있으므로 "성능이 좋아 보이니까" 바로 도입하지 않고, 목표
  플랫폼에서 Unreal Insights와 벤치마크로 확인한 뒤 결정한다.
