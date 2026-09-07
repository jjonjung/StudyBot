# 발표자료 — 배관 태그 조회, 왜 자료구조가 성능을 좌우하는가

AutoLISP로 처음 학습한 "해시 테이블의 충돌과 분포" 개념을, C++로 재구현하고
실측 수치까지 뽑아 정리했다. 발표 흐름: **문제 상황 → 나쁜 예 → 좋은 예 →
실측 비교 → UE5 실전 적용**.

---

## 1. 문제 상황

반도체 팹 배관 P&ID 도면 한 장에는 수천~수만 개의 배관 세그먼트가 있고,
각 세그먼트는 태그로 식별된다.

```
P-101-A-2FL   (Zone 101, 배관 A, 2층)
P-101-B-2FL   (Zone 101, 배관 B, 2층)
P-205-A-1FL   (Zone 205, 배관 A, 1층)
```

작업자가 도면에서 태그를 클릭하거나, 스크립트가 특정 Zone의 배관을 일괄
처리(색상 변경, 간섭 체크, BOM 집계)할 때마다 **"태그 → 데이터" 조회**가
반복된다. 이 조회가 느리면 도면이 커질수록 매크로 실행 시간이 체감될 정도로
늘어난다.

---

## 2. 나쁜 예 — 선형 탐색 & "절반만 반영한 hash"

### 2-1. 순수 선형 탐색

가장 흔히 짜는 방식: 리스트를 처음부터 끝까지 훑으며 태그를 비교한다.
배관이 200개면 문제없지만, 20,000개가 되면 평균 10,000번 비교가 필요하다.

```cpp
// PipeTagLookup.cpp — FindPipeLinear()
for (const FPipeEntry& Entry : Pipes)
{
    IncCompare();
    if (Entry.Tag == Tag) return &Entry;
}
```

### 2-2. "해시테이블"이라 부르지만 함정이 있는 버전

더 흔하고 더 위험한 실수: hash를 쓰긴 쓰는데 **Zone ID만으로 hash를 계산**한다.

```cpp
// PipeTagLookup.cpp — BadHashZoneOnly()
static int32_t BadHashZoneOnly(const std::string& Zone, int32_t NumBuckets)
{
    return std::stoi(Zone) % NumBuckets;
}
```

태그의 identity는 **(Zone, PipeId, Floor) 세 개**인데, hash가 Zone 하나만
본다. 같은 Zone에 배관이 몰린 도면(실무에서 매우 흔함)에서는 특정 버킷
하나에 수천 개가 쌓여서 — **"해시테이블"이라는 이름만 쓰고 사실상
선형 탐색과 다를 게 없어진다.**

> 이게 오늘 배운 "hash는 부분 key만 쓰는데 identity는 여러 필드"라는
> 함정과 정확히 같은 패턴이다.

---

## 3. 좋은 예 — 복합키 hash

태그 전체(Zone+PipeId+Floor를 합친 문자열)를 hash 입력으로 쓴다.

```cpp
// PipeTagLookup.cpp — CharHash() / PipeHash()
static uint32_t CharHash(const std::string& Str)
{
    uint64_t H = 0;
    for (const char C : Str)
        H = (H * 31 + (unsigned char)C) % 2147483647ULL;
    return (uint32_t)H;
}
```

다항 해시(polynomial hash) — 문자 하나하나를 `h = h*31 + c`로 누적한다.
이 방식이 지키는 세 가지 hash 계약:

| 원칙 | 이 코드에서 지키는 방법 |
|---|---|
| 같은 key는 항상 같은 hash | 태그 문자열 전체를 입력으로 사용 (부분키 아님) |
| 다른 key는 (대체로) 다른 hash | 문자 단위 누적합이므로 한두 글자만 달라도 값이 크게 갈림 |
| 실제 데이터에서 특정 버킷에 몰리지 않음 | Zone뿐 아니라 PipeId, Floor까지 반영 → 같은 Zone에 배관이 몰려도 버킷은 분산 |

```cpp
// PipeTagLookup.cpp — FindPipeGoodHash()
const int32_t Idx = PipeHash(Tag, NumBuckets);
for (const FPipeEntry& Entry : Buckets[Idx])
{
    IncCompare();
    if (Entry.Tag == Tag) return &Entry;
}
```

버킷 하나만 열어보고, 그 안에서만 비교한다. 버킷이 고르게 분산돼 있으면
버킷당 원소 수는 평균 `전체 개수 / 버킷 수`로 줄어든다.

---

## 4. 실측 비교 — 세 방식을 같은 데이터로 벤치마크

실제 팹 도면 특성을 재현: 20개 Zone에 배관 총 **10,400개**를 만들되,
앞쪽 Zone(101~104)에 배관이 몰리도록 불균등하게 분포시켰다
(Zone 101에 3,000개, Zone 210에는 80개 — 실무에서 흔한 쏠림 재현).

버킷 64개, 조회 타깃은 가장 몰린 Zone 101의 마지막 항목(`P-101-2999-2FL`)으로
worst-case에 가깝게 설정했다.

### 비교 횟수 결과

| 방식 | 비교 횟수 | 비고 |
|---|---|---|
| [1] 선형 탐색 | **3,000회** | 리스트를 앞에서부터 순서대로 훑음 |
| [2] Zone만 hash (나쁜 예) | **3,000회** | Zone 101 버킷에 3,000개가 그대로 몰려 선형 탐색과 동일 |
| [3] 복합키 hash (좋은 예) | **22회** | 버킷이 균등 분산되어 버킷당 평균 ~163개 중 22회만에 히트 |

**개선율: 선형/나쁜 hash 대비 약 136배**

### 버킷 분포 비교 (64개 버킷 기준)

| | 최대 버킷 크기 | 최소 버킷 크기 | 사용된 버킷 수 |
|---|---|---|---|
| 나쁜 hash (Zone만) | 3,000 | 0 | 20 / 64 |
| 좋은 hash (복합키) | 195 | 130 | 64 / 64 |

나쁜 hash는 버킷 64개 중 **20개만 사용**되고(Zone 종류가 20개뿐이라
그 이상 분산될 수가 없다), 그마저도 한 버킷에 3,000개가 몰린다.
좋은 hash는 **버킷 64개를 전부 사용**하고, 버킷 크기가 130~195 사이로
고르게 퍼진다 — 이게 "분포가 성능을 만든다"는 걸 숫자로 보여주는 부분이다.

> 재현 스크립트: `PipeTagLookup.cpp` 를 컴파일해 실행하면 동일한 로직으로
> 같은 결과를 직접 확인할 수 있다. (`g++ -std=c++17 -O2 PipeTagLookup.cpp -o pipe_bench`)

---

## 5. AutoLISP → C++ → UE5, 세 버전의 관계

같은 알고리즘을 세 가지 레벨로 구현해봤다 — "개념은 언어에 무관하다"는 걸
보여주기 위해서다.

| 파일 | 역할 |
|---|---|
| [bad_linear_search.lsp](bad_linear_search.lsp) / [good_hash_lookup.lsp](good_hash_lookup.lsp) / [bench.lsp](bench.lsp) | **원본** — AutoCAD/AutoLISP 환경, 실제 배관 CAD 워크플로우에서 학습한 문제 상황 |
| [PipeTagLookup.cpp](PipeTagLookup.cpp) | **재구현** — 표준 C++만으로 동일 로직 이식. 해시/버킷을 직접 구현해 "내부적으로 무슨 일이 일어나는지" 눈으로 확인 가능 |
| [PipeTagLookup_UE5.h](PipeTagLookup_UE5.h) | **실전 적용** — UE5 `TMap`을 쓰면 해시 함수·버킷 배열·충돌 처리를 엔진이 대신 해준다. `good_hash_lookup.lsp`가 80줄 넘게 손으로 짠 걸 `TMap::Add`/`TMap::Find` 두 줄로 대체 |

```cpp
// PipeTagLookup_UE5.h — 전체 조회 로직이 이 두 줄로 끝난다
void AddPipe(const FPipeEntry& Entry) { PipeMap.Add(Entry.Tag, Entry); }
const FPipeEntry* FindPipe(const FString& Tag) const { return PipeMap.Find(Tag); }
```

**핵심 메시지**: `TMap`을 쓴다고 저절로 빨라지는 게 아니다. `TMap`이
내부적으로 하는 일(복합키를 hash 입력으로 쓰고, 버킷에 고르게 분산시키는 것)을
`good_hash_lookup.lsp`에서 직접 짜봤기 때문에, 나쁜 예(`BadHashZoneOnly`)처럼
Key 설계를 잘못하면 `TMap`을 쓰고도 성능이 안 나올 수 있다는 걸 안다.
즉 "왜 빠른지"를 알아야 실전에서 "왜 안 빨라지는지"도 진단할 수 있다.

이 원칙은 [../ContainerSelection/README.md](../ContainerSelection/README.md)의
"자료구조는 Big-O가 아니라 실제 연산 패턴으로 고른다"는 기준과 정확히
같은 결론으로 이어진다 — 여기서는 "hash를 쓰더라도 identity 전체를
반영해야 한다"는 조건이 그 실제 연산 패턴의 한 사례였던 것이다.

---

## 6. 발표 요약 한 줄

> **"해시테이블을 썼다"가 아니라 "hash 함수가 identity 전체를 반영하고
> 버킷이 고르게 분산되는가"가 실제 성능을 만든다 — 이번 벤치마크에서는
> 그 차이가 136배로 나타났다.**
