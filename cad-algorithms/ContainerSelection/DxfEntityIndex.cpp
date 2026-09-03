// DxfEntityIndex.cpp

#include "DxfEntityIndex.h"
#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"

bool FDxfEntityIndex::AddEntity(const FDxfEntity& Entity)
{
	if (TagToIndex.Contains(Entity.Tag))
	{
		return false; // 태그는 identity이므로 중복 추가는 거부
	}

	const int32 NewIndex = Entities.Add(Entity);
	TagToIndex.Add(Entity.Tag, NewIndex);
	bSortedIndexDirty = true; // 정렬 인덱스는 이제 stale
	return true;
}

const FDxfEntity* FDxfEntityIndex::FindByTag(const FString& Tag) const
{
	if (const int32* FoundIndex = TagToIndex.Find(Tag))
	{
		return &Entities[*FoundIndex];
	}
	return nullptr;
}

void FDxfEntityIndex::BuildSortedIndex()
{
	SortedByX.Reset(Entities.Num());
	for (int32 i = 0; i < Entities.Num(); ++i)
	{
		SortedByX.Emplace(Entities[i].X, i);
	}

	// X좌표 오름차순 정렬 — O(N log N), 도면 편집이 끝난 뒤 1회만 지불하는 비용
	Algo::SortBy(SortedByX, &TPair<double, int32>::Key);

	bSortedIndexDirty = false;
}

TArray<const FDxfEntity*> FDxfEntityIndex::FindByXRange(double MinX, double MaxX) const
{
	TArray<const FDxfEntity*> Result;

	if (bSortedIndexDirty)
	{
		// 정렬 인덱스가 최신이 아니면 범위 검색 결과를 보장할 수 없다.
		// 실무에서는 여기서 ensure/로그를 남기고 즉시 재빌드하거나 빈 결과를 반환한다.
		return Result;
	}

	// 이진 탐색으로 MinX가 들어갈 첫 위치를 O(log N)에 찾는다.
	const int32 StartIdx = Algo::LowerBoundBy(SortedByX, MinX, &TPair<double, int32>::Key);

	// 그 지점부터는 이미 정렬돼 있으므로 MaxX를 넘는 순간 멈추면 된다 — 선형 탐색과
	// 달리 "전체를 다 봐야 하는" 경우가 없다.
	for (int32 i = StartIdx; i < SortedByX.Num(); ++i)
	{
		const double X = SortedByX[i].Key;
		if (X > MaxX)
		{
			break;
		}
		Result.Add(&Entities[SortedByX[i].Value]);
	}

	return Result;
}

TArray<FString> FDxfEntityIndex::CollectUniqueTagsInLayer(const FString& Layer) const
{
	TSet<FString> Seen;
	TArray<FString> Result;
	Result.Reserve(Entities.Num());

	for (const FDxfEntity& Entity : Entities)
	{
		if (Entity.Layer != Layer)
		{
			continue;
		}

		// TSet::Contains -> Add 두 번 호출 대신, Add의 반환값(bAlreadyInSet)으로 한 번에 처리
		bool bAlreadyInSet = false;
		Seen.Add(Entity.Tag, &bAlreadyInSet);
		if (!bAlreadyInSet)
		{
			Result.Add(Entity.Tag);
		}
	}

	return Result;
}
