;;; ============================================================
;;; bench.lsp
;;; bad_linear_search.lsp / good_hash_lookup.lsp 를 같이 로드한 뒤
;;; 더미 배관 태그를 생성해 조회 비교 횟수를 눈으로 비교하는 벤치마크
;;;
;;; 사용법 (AutoCAD 명령창):
;;;   (load "bad_linear_search.lsp")
;;;   (load "good_hash_lookup.lsp")
;;;   (load "bench.lsp")
;;;   PIPE-BENCH
;;;
;;; 실제 반도체 팹 도면 특성을 흉내내어, 20개 Zone에 배관을
;;; 불균등하게 분포시킵니다 (일부 Zone에 배관이 몰리는 흔한 상황).
;;; ============================================================

;; ---------------------------------------------------------------
;; 더미 배관 태그 생성
;;   zone-weights: 각 zone에 배관이 몇 개 있을지 (실무처럼 불균등하게)
;;   예: Zone 101에 3000개, Zone 205에 50개 ... 몰림 재현
;; ---------------------------------------------------------------
(defun make-dummy-pipes (/ zones weights pipe-alist zone weight i pid tag)
  (setq zones   '("101" "102" "103" "104" "105" "106" "107" "108" "109" "110"
                   "201" "202" "203" "204" "205" "206" "207" "208" "209" "210"))
  ;; 몰림을 재현: 앞쪽 몇 개 Zone에 배관이 훨씬 많음 (실제 팹 도면 특성)
  (setq weights '(3000 2500 2000 1500 100  100  100  100  100  100
                  80    80   80   80   80   80   80   80   80   80))
  (setq pipe-alist '())
  (mapcar
    '(lambda (zone weight)
       (setq i 0)
       (repeat weight
         (setq pid (itoa i))
         (setq tag (strcat "P-" zone "-" pid "-2FL"))
         (setq pipe-alist (cons (cons tag (list zone pid)) pipe-alist))
         (setq i (1+ i))
       )
     )
    zones weights
  )
  pipe-alist
)

;; ---------------------------------------------------------------
;; PIPE-BENCH 명령
;; ---------------------------------------------------------------
(defun c:PIPE-BENCH (/ pipes total num-buckets
                       target-tag target-zone
                       bad-buckets good-buckets
                       t1 t2 t3)

  (setq pipes (make-dummy-pipes))
  (setq total (length pipes))
  (setq num-buckets 64)

  (princ (strcat "\n=== 배관 태그 조회 벤치마크 (총 " (itoa total) "개 배관) ===\n"))

  ;; 실제로 자주 찾게 되는 타깃: 배관이 몰려 있는 Zone 101의 마지막 항목
  (setq target-tag "P-101-2999-2FL")
  (setq target-zone "101")

  ;; --- 1) 순수 선형 탐색 ---
  (setq *compare-count* 0)
  (find-pipe-linear target-tag pipes)
  (setq t1 *compare-count*)
  (princ (strcat "\n[1] 선형 탐색 (alist)              : " (itoa t1) " 회 비교"))

  ;; --- 2) Zone ID만으로 만든 나쁜 hash ---
  (setq bad-buckets (build-bad-hash pipes num-buckets))
  (setq *compare-count* 0)
  (find-pipe-bad-hash target-tag bad-buckets num-buckets)
  (setq t2 *compare-count*)
  (princ (strcat "\n[2] Zone만 hash (편중 발생)         : " (itoa t2) " 회 비교"
                 "   <- Zone 101 버킷에 배관이 몰려있어 사실상 선형 탐색"))

  ;; --- 3) 복합키(Zone+PipeId+Floor) 좋은 hash ---
  (setq good-buckets (build-pipe-hash pipes num-buckets))
  (setq *compare-count* 0)
  (find-pipe-hash target-tag good-buckets num-buckets)
  (setq t3 *compare-count*)
  (princ (strcat "\n[3] 복합키 hash (균등 분산)          : " (itoa t3) " 회 비교"))

  (princ "\n\n--- 버킷 분포 비교 (버킷당 배관 개수, 64개 버킷) ---")
  (princ "\n나쁜 hash (Zone만)  : ")
  (princ (bucket-distribution bad-buckets))
  (princ "\n좋은 hash (복합키)  : ")
  (princ (bucket-distribution good-buckets))

  (princ "\n\n--- Zone 101 일괄 조회 (getall-by-zone) 비교 ---")
  (setq *compare-count* 0)
  (getall-by-zone-linear target-zone pipes)
  (princ (strcat "\n선형 방식 : " (itoa *compare-count*) " 회 비교 (전체 " (itoa total) "개 순회)"))

  (setq *compare-count* 0)
  (getall-by-zone-hash target-zone good-buckets)
  (princ (strcat "\n해시 방식 : " (itoa *compare-count*) " 회 비교 (버킷 순회, 조회 자체는 동일하지만 이후 태그 단위 조회는 [3]처럼 평균 O(1))"))

  (princ "\n\n=== 결론: 태그 하나 찾는 데 필요한 비교 횟수가 ===")
  (princ (strcat "\n선형: " (itoa t1) "  vs  나쁜 hash: " (itoa t2) "  vs  좋은 hash: " (itoa t3)))
  (princ "\n도면 규모(배관 수)가 커질수록 이 격차는 더 벌어집니다.\n")
  (princ)
)

(princ "\n[bench.lsp] loaded: PIPE-BENCH 명령을 입력하세요.")
(princ)
