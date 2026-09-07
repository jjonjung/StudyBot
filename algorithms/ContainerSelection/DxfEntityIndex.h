// DxfEntityIndex.h
//
// 2D DXF CAD 설계 예제: 도면 하나에 배관/심볼/텍스트 엔티티가 수만 개 있다.
//
// 이 시스템에서 실제로 벌어지는 연산 4가지와 그에 맞는 자료구조:
//
//   1. 레이어 전체를 export하거나 렌더링할 때 -> 전체 순회
//        => TArray<FDxfEntity> (연속 메모리, 캐시 지역성)
//
//   2. 사용자가 도면에서 태그를 클릭하거나 스크립트가 태그로 조회할 때 -> ID lookup
//        => TMap<FString,int32> (태그 -> 배열 인덱스)
//
//   3. "X좌표 범위 안의 엔티티만 뽑기" 같은 범위 검색을 반복할 때,
//      완성된 도면(삽입/삭제가 거의 없는 상태)이라면 -> 정렬 유지 + Binary Search
//        => 정렬 TArray<FDxfEntity> + Algo::BinarySearch / Algo::LowerBound
//      (편집 중인 도면처럼 삽입·삭제가 잦다면 정렬 비용이 더 커서 오히려 손해다.
//       이 예제는 "설계 완료 후 도면 검토/BOM 집계" 단계를 가정한다.)
//
//   4. BOM 집계나 간섭 체크 스캔에서 "이미 처리한 태그인지" 확인할 때 -> 존재 여부만
//        => TSet<FString>

#pragma once

#include "CoreMinimal.h"

struct FDxfEntity
{
	FString Tag;          // 예: "P-101-A-2FL"
	FString Layer;        // 예: "PIPING-2FL"
	double X = 0.0;        // 도면 좌표계 X (정렬 기준 키)
	double Y = 0.0;
};

/**
 * DXF 엔티티 색인. TArray를 정답 데이터로 두고, 목적별로
 * TMap / 정렬 인덱스 / TSet을 얹는 구조는 FabEquipmentRegistry와 동일한 패턴이다.
 */
class FDxfEntityIndex
{
public:
	/** 엔티티 추가. 태그 중복은 허용하지 않는다. */
	bool AddEntity(const FDxfEntity& Entity);

	/** 레이어 전체 export/렌더링용 순회 대상 (그대로 참조 반환) */
	const TArray<FDxfEntity>& GetAllEntities() const { return Entities; }

	/**
	 * 태그로 단건 조회 — Contains+operator[] 대신 Find 한 번.
	 * AutoLISP 예제(PipeTagLookup)의 해시 조회를 UE5 TMap으로 옮긴 버전과 동일한 발상.
	 */
	const FDxfEntity* FindByTag(const FString& Tag) const;

	/**
	 * 정렬 인덱스를 최신 상태로 굳힌다 (도면 편집이 끝난 뒤 1회 호출).
	 * 삽입/삭제 때마다 정렬을 유지하면 비용이 크므로, "완성된 도면을
	 * 반복 조회"하는 워크플로우에서만 이 방식을 쓴다.
	 */
	void BuildSortedIndex();

	/**
	 * X좌표 [MinX, MaxX] 범위 안의 엔티티를 Binary Search로 찾는다.
	 * BuildSortedIndex()를 먼저 호출해야 한다 — 그렇지 않으면 정렬 가정이 깨져
	 * 잘못된 결과를 낼 수 있다.
	 */
	TArray<const FDxfEntity*> FindByXRange(double MinX, double MaxX) const;

	/**
	 * BOM 집계 스캔 예시: 레이어 전체를 순회하되, TSet으로 "이미 집계한 태그"를
	 * 걸러낸다. 같은 태그가 여러 뷰포트에 중복 등장하는 DXF 특성 때문에 필요하다.
	 */
	TArray<FString> CollectUniqueTagsInLayer(const FString& Layer) const;

private:
	/** 정답 데이터: 삽입 순서 그대로. 전체 순회의 대상. */
	TArray<FDxfEntity> Entities;

	/** 태그 -> Entities 인덱스. 클릭/스크립트 조회 전용. */
	TMap<FString, int32> TagToIndex;

	/**
	 * X좌표 기준 정렬된 (X, EntitiesIndex) 쌍.
	 * BuildSortedIndex() 호출 시점 스냅샷이며, 이후 Entities가 바뀌면 stale해진다.
	 */
	TArray<TPair<double, int32>> SortedByX;
	bool bSortedIndexDirty = true;
};
