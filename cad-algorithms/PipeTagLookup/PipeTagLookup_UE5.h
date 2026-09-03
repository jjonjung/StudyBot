// PipeTagLookup_UE5.h
//
// PipeTagLookup.cpp(순수 C++)와 동일한 로직을 UE5 컨테이너(TArray/TMap)로
// 옮긴 버전. AutoLISP로 손수 구현했던 "해시 버킷"을 UE5에서는 TMap이
// 대신 처리해준다는 것을 보여주는 게 목적이다.
//
// 즉 이 파일이 곧 "좋은 예"의 실전 버전이다 — 버킷/해시 함수를 직접 짤 필요 없이
// TMap<FString, FPipeEntry>에 넣기만 하면 good_hash_lookup.lsp가 하던 일을
// 엔진이 대신 해준다. (ContainerSelection/README.md의 "TMap: Key->Value 임의
// 조회가 핵심일 때" 항목과 정확히 일치하는 사례)

#pragma once

#include "CoreMinimal.h"

USTRUCT()
struct FPipeEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString Tag;   // 예: "P-101-42-2FL"

	UPROPERTY()
	FString Zone;  // 예: "101"

	UPROPERTY()
	FString PipeId; // 예: "42"
};

/**
 * good_hash_lookup.lsp가 손으로 구현한 "복합키 해시 + 버킷 배열"을
 * TMap 하나로 대체한 버전.
 *
 * AutoLISP 버전 대비 이 클래스가 짧은 이유: 해시 함수, 버킷 배열,
 * 충돌 처리, 분포 계산을 전부 TMap이 내부적으로 해결해주기 때문이다.
 * "직접 구현해야 했던 것"과 "엔진 컨테이너가 대신 해주는 것"의 경계를
 * 보여주는 것이 이 파일의 발표 포인트.
 */
class FPipeTagRegistry
{
public:
	/** 배관 추가 — 태그가 Key, TMap이 내부에서 해시/버킷을 알아서 처리 */
	void AddPipe(const FPipeEntry& Entry)
	{
		PipeMap.Add(Entry.Tag, Entry);
	}

	/**
	 * 태그로 단건 조회 — Contains+operator[] 대신 Find 한 번.
	 * good_hash_lookup.lsp의 find-pipe-hash와 동일한 역할이지만,
	 * 버킷 인덱스 계산/순회를 TMap이 대신한다.
	 */
	const FPipeEntry* FindPipe(const FString& Tag) const
	{
		return PipeMap.Find(Tag);
	}

	/**
	 * Zone 단위 일괄 조회 — getall-by-zone-hash 대응.
	 * TMap은 range-for로 전체 순회가 가능하므로, 필요하면 이렇게 필터링한다.
	 * (Zone별로 자주 모아야 한다면, PipeMap과 별도로
	 *  TMap<FString, TArray<FString>> ZoneToTags 색인을 하나 더 두는 편이
	 *  ContainerSelection 패턴과 일관된다 — 여기서는 단순화를 위해 생략)
	 */
	TArray<const FPipeEntry*> GetAllByZone(const FString& ZoneId) const
	{
		TArray<const FPipeEntry*> Result;
		for (const auto& Pair : PipeMap)
		{
			if (Pair.Value.Zone == ZoneId)
			{
				Result.Add(&Pair.Value);
			}
		}
		return Result;
	}

	int32 Num() const { return PipeMap.Num(); }

private:
	/**
	 * Tag -> FPipeEntry 전체를 직접 저장.
	 * (ContainerSelection 예제들처럼 "TArray 정답 데이터 + TMap 색인"으로도
	 *  짤 수 있지만, 여기서는 배관 태그 조회 자체가 유일한 hot path이므로
	 *  TMap 하나로 충분하다 — 이것도 "실제 연산 패턴에 맞춰 고른다"는
	 *  README 원칙의 사례다.)
	 */
	TMap<FString, FPipeEntry> PipeMap;
};
