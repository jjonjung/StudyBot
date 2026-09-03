// PipeTagLookup.cpp
//
// bad_linear_search.lsp / good_hash_lookup.lsp / bench.lsp (AutoLISP)를
// C++11 표준 라이브러리만으로 그대로 재구현한 버전.
//
// AutoLISP 원본은 alist(연상 리스트)와 "리스트의 리스트"로 해시 버킷을 흉내냈지만,
// 개념은 동일하다: 배관 태그(P-<Zone>-<PipeId>-<Floor>) -> 데이터 조회를
//   1) 선형 탐색
//   2) Zone ID만으로 만든 나쁜 hash (편중 발생)
//   3) Zone+PipeId+Floor 복합키 hash (균등 분산)
// 세 가지 방식으로 비교하고, 비교 횟수를 계측해 실제 성능 차이를 눈으로 보여준다.
//
// 빌드 (컴파일러가 있는 환경에서):
//   g++ -std=c++17 -O2 PipeTagLookup.cpp -o pipe_bench
//   ./pipe_bench
//
// UE5 프로젝트에 통합할 경우 std::string 대신 FString/FName, std::vector 대신
// TArray로 바꾸면 된다 — 로직은 동일하다. (컨테이너 선택 자체는
// ../ContainerSelection/README.md 참고)

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

// -----------------------------------------------------------------
// 공용: 배관 데이터 및 비교 횟수 계측
// -----------------------------------------------------------------

struct FPipeEntry
{
	std::string Tag;   // 예: "P-101-42-2FL"
	std::string Zone;  // 예: "101"
	std::string PipeId;// 예: "42"
};

static int64_t GCompareCount = 0;

static inline void IncCompare()
{
	++GCompareCount;
}

// "P-101-42-2FL" -> "101"
static std::string ExtractZone(const std::string& Tag)
{
	// 첫 번째 '-'와 두 번째 '-' 사이가 Zone
	const size_t First = Tag.find('-');
	const size_t Second = Tag.find('-', First + 1);
	return Tag.substr(First + 1, Second - First - 1);
}

// -----------------------------------------------------------------
// 1) 나쁜 예 (bad_linear_search.lsp 대응) — 선형 탐색
// -----------------------------------------------------------------

static const FPipeEntry* FindPipeLinear(const std::string& Tag, const std::vector<FPipeEntry>& Pipes)
{
	for (const FPipeEntry& Entry : Pipes)
	{
		IncCompare();
		if (Entry.Tag == Tag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

// -----------------------------------------------------------------
// 2) 나쁜 예 — Zone ID만으로 만든 hash (identity를 일부만 반영하는 함정)
// -----------------------------------------------------------------

static int32_t BadHashZoneOnly(const std::string& Zone, int32_t NumBuckets)
{
	return std::stoi(Zone) % NumBuckets;
}

static std::vector<std::vector<FPipeEntry>> BuildBadHash(const std::vector<FPipeEntry>& Pipes, int32_t NumBuckets)
{
	std::vector<std::vector<FPipeEntry>> Buckets(NumBuckets);
	for (const FPipeEntry& Entry : Pipes)
	{
		const int32_t Idx = BadHashZoneOnly(Entry.Zone, NumBuckets);
		Buckets[Idx].push_back(Entry);
	}
	return Buckets;
}

static const FPipeEntry* FindPipeBadHash(const std::string& Tag, const std::vector<std::vector<FPipeEntry>>& Buckets, int32_t NumBuckets)
{
	const std::string Zone = ExtractZone(Tag);
	const int32_t Idx = BadHashZoneOnly(Zone, NumBuckets);
	for (const FPipeEntry& Entry : Buckets[Idx])
	{
		IncCompare();
		if (Entry.Tag == Tag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

// -----------------------------------------------------------------
// 3) 좋은 예 (good_hash_lookup.lsp 대응) — 복합키(Zone+PipeId+Floor) 다항 해시
//    hash = (hash * 31 + char) 를 태그 문자열 전체에 대해 누적
// -----------------------------------------------------------------

static uint32_t CharHash(const std::string& Str)
{
	uint64_t H = 0;
	for (const char C : Str)
	{
		H = (H * 31 + static_cast<unsigned char>(C)) % 2147483647ULL;
	}
	return static_cast<uint32_t>(H);
}

static int32_t PipeHash(const std::string& Tag, int32_t NumBuckets)
{
	return static_cast<int32_t>(CharHash(Tag) % static_cast<uint32_t>(NumBuckets));
}

static std::vector<std::vector<FPipeEntry>> BuildGoodHash(const std::vector<FPipeEntry>& Pipes, int32_t NumBuckets)
{
	std::vector<std::vector<FPipeEntry>> Buckets(NumBuckets);
	for (const FPipeEntry& Entry : Pipes)
	{
		const int32_t Idx = PipeHash(Entry.Tag, NumBuckets);
		Buckets[Idx].push_back(Entry);
	}
	return Buckets;
}

static const FPipeEntry* FindPipeGoodHash(const std::string& Tag, const std::vector<std::vector<FPipeEntry>>& Buckets, int32_t NumBuckets)
{
	const int32_t Idx = PipeHash(Tag, NumBuckets);
	for (const FPipeEntry& Entry : Buckets[Idx])
	{
		IncCompare();
		if (Entry.Tag == Tag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

// -----------------------------------------------------------------
// 버킷 분포 확인 (bench.lsp의 bucket-distribution 대응)
// -----------------------------------------------------------------

static void PrintBucketDistributionSummary(const std::vector<std::vector<FPipeEntry>>& Buckets, const char* Label)
{
	size_t MaxSize = 0, MinSize = SIZE_MAX, NonZero = 0;
	for (const auto& Bucket : Buckets)
	{
		MaxSize = std::max(MaxSize, Bucket.size());
		MinSize = std::min(MinSize, Bucket.size());
		if (!Bucket.empty())
		{
			++NonZero;
		}
	}
	std::cout << Label << " : max=" << MaxSize << ", min=" << MinSize
		<< ", 사용된 버킷 수=" << NonZero << "/" << Buckets.size() << "\n";
}

// -----------------------------------------------------------------
// 더미 배관 태그 생성 (bench.lsp의 make-dummy-pipes 대응)
// 실제 팹 도면처럼 앞쪽 Zone(101~104)에 배관이 몰리도록 불균등 생성
// -----------------------------------------------------------------

static std::vector<FPipeEntry> MakeDummyPipes()
{
	const std::vector<std::string> Zones = {
		"101","102","103","104","105","106","107","108","109","110",
		"201","202","203","204","205","206","207","208","209","210"
	};
	const std::vector<int32_t> Weights = {
		3000,2500,2000,1500,100,100,100,100,100,100,
		80,80,80,80,80,80,80,80,80,80
	};

	std::vector<FPipeEntry> Pipes;
	for (size_t Z = 0; Z < Zones.size(); ++Z)
	{
		for (int32_t i = 0; i < Weights[Z]; ++i)
		{
			const std::string PipeId = std::to_string(i);
			FPipeEntry Entry;
			Entry.Zone = Zones[Z];
			Entry.PipeId = PipeId;
			Entry.Tag = "P-" + Zones[Z] + "-" + PipeId + "-2FL";
			Pipes.push_back(std::move(Entry));
		}
	}
	return Pipes;
}

// -----------------------------------------------------------------
// PIPE-BENCH 명령 대응
// -----------------------------------------------------------------

int main()
{
	const std::vector<FPipeEntry> Pipes = MakeDummyPipes();
	const size_t Total = Pipes.size();
	const int32_t NumBuckets = 64;

	std::cout << "=== 배관 태그 조회 벤치마크 (총 " << Total << "개 배관) ===\n\n";

	// 실무에서 자주 찾게 되는 타깃: 배관이 몰려 있는 Zone 101의 마지막 항목
	const std::string TargetTag = "P-101-2999-2FL";

	// [1] 순수 선형 탐색
	GCompareCount = 0;
	FindPipeLinear(TargetTag, Pipes);
	const int64_t T1 = GCompareCount;
	std::cout << "[1] 선형 탐색 (vector)              : " << T1 << " 회 비교\n";

	// [2] Zone ID만으로 만든 나쁜 hash
	const auto BadBuckets = BuildBadHash(Pipes, NumBuckets);
	GCompareCount = 0;
	FindPipeBadHash(TargetTag, BadBuckets, NumBuckets);
	const int64_t T2 = GCompareCount;
	std::cout << "[2] Zone만 hash (편중 발생)          : " << T2
		<< " 회 비교   <- Zone 101 버킷에 배관이 몰려 사실상 선형 탐색\n";

	// [3] 복합키(Zone+PipeId+Floor) 좋은 hash
	const auto GoodBuckets = BuildGoodHash(Pipes, NumBuckets);
	GCompareCount = 0;
	FindPipeGoodHash(TargetTag, GoodBuckets, NumBuckets);
	const int64_t T3 = GCompareCount;
	std::cout << "[3] 복합키 hash (균등 분산)           : " << T3 << " 회 비교\n";

	std::cout << "\n--- 버킷 분포 비교 (" << NumBuckets << "개 버킷) ---\n";
	PrintBucketDistributionSummary(BadBuckets, "나쁜 hash (Zone만)");
	PrintBucketDistributionSummary(GoodBuckets, "좋은 hash (복합키)");

	std::cout << "\n=== 결론: 태그 하나 찾는 데 필요한 비교 횟수 ===\n";
	std::cout << "선형: " << T1 << "  vs  나쁜 hash: " << T2 << "  vs  좋은 hash: " << T3 << "\n";
	if (T3 > 0)
	{
		std::cout << "선형 대비 개선율: " << (static_cast<double>(T1) / T3) << "배\n";
	}
	std::cout << "도면 규모(배관 수)가 커질수록 이 격차는 더 벌어진다.\n";

	return 0;
}
