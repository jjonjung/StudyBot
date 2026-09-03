;;; ============================================================
;;; good_hash_lookup.lsp
;;; 좋은 예: Zone+PipeId+Floor 복합키로 hash를 계산해
;;;         버킷에 고르게 분산시키는 배관 태그 조회
;;; ============================================================
;;;
;;; 오늘 학습 내용 적용:
;;;   - "같은 key는 안정적으로 같은 hash를 만든다"
;;;     -> 태그 문자열 전체를 hash 입력으로 사용 (부분키 아님)
;;;   - "equality로 같은 key는 반드시 같은 hash를 가진다"
;;;     -> 태그 문자열이 다르면 다른 값이 나올 수 있는 문자 단위
;;;        누적합 해시 사용 (아래 char-hash)
;;;   - "실제 데이터에서 특정 bucket으로 과도하게 몰리지 않는다"
;;;     -> Zone뿐 아니라 PipeId, Floor까지 hash에 반영되므로
;;;        같은 Zone에 배관이 몰려도 버킷은 골고루 분산됨
;;;   - "hash 계산 비용 자체가 지나치게 크지 않다"
;;;     -> 문자 하나씩 도는 단순 다항 해시(polynomial hash) 사용
;;; ============================================================

(setq *compare-count* 0)

(defun inc-compare ()
  (setq *compare-count* (1+ *compare-count*))
)

;; ---------------------------------------------------------------
;; 문자열 전체(Zone+PipeId+Floor 복합키)를 대상으로 한 다항 해시
;; hash = (hash * 31 + char-code) 를 각 문자에 대해 누적
;; ---------------------------------------------------------------
(defun char-hash (str / h i n c)
  (setq h 0)
  (setq n (strlen str))
  (setq i 1)
  (repeat n
    (setq c (ascii (substr str i 1)))
    (setq h (rem (+ (* h 31) c) 2147483647))
    (setq i (1+ i))
  )
  h
)

(defun pipe-hash (tag num-buckets)
  (rem (char-hash tag) num-buckets)
)

;; ---------------------------------------------------------------
;; 해시테이블 구성
;;   pipe-alist 형식: (("P-101-A-2FL" . entity-data) ...)
;;   buckets: num-buckets 크기의 리스트, 각 원소는 (tag . data)의 리스트
;; ---------------------------------------------------------------
(defun make-empty-buckets (n / result i)
  (setq result '())
  (setq i 0)
  (repeat n
    (setq result (cons '() result))
    (setq i (1+ i))
  )
  result
)

(defun bucket-push (buckets idx entry / result i cur)
  (setq result '())
  (setq i 0)
  (foreach cur buckets
    (if (= i idx)
      (setq result (cons (cons entry cur) result))
      (setq result (cons cur result))
    )
    (setq i (1+ i))
  )
  (reverse result)
)

(defun build-pipe-hash (pipe-alist num-buckets / buckets entry tag idx)
  (setq buckets (make-empty-buckets num-buckets))
  (foreach entry pipe-alist
    (setq tag (car entry))
    (setq idx (pipe-hash tag num-buckets))
    (setq buckets (bucket-push buckets idx entry))
  )
  buckets
)

;; ---------------------------------------------------------------
;; 조회: 버킷 하나만 열어보고, 그 안에서만 equality 비교
;;   (버킷이 잘 분산되어 있으면 이 안의 원소 수는 평균 N/num-buckets)
;; ---------------------------------------------------------------
(defun find-pipe-hash (tag buckets num-buckets / idx bucket lst found)
  (setq idx (pipe-hash tag num-buckets))
  (setq bucket (nth idx buckets))
  (setq lst bucket)
  (setq found nil)
  (while (and lst (not found))
    (inc-compare)
    (if (= (car (car lst)) tag)
      (setq found (car lst))
      (setq lst (cdr lst))
    )
  )
  found
)

;; ---------------------------------------------------------------
;; Zone 단위 일괄 조회도 동일한 해시테이블을 재사용 가능
;; (전체 버킷을 순회하되, 각 버킷 안에서만 필터링 -> 여전히
;;  버킷 개수 자체가 배관 수와 무관하게 고정이므로 유리)
;; ---------------------------------------------------------------
(defun getall-by-zone-hash (zone-id buckets / result bucket entry tag)
  (setq result '())
  (foreach bucket buckets
    (foreach entry bucket
      (inc-compare)
      (setq tag (car entry))
      (if (= (extract-zone tag) zone-id)
        (setq result (cons entry result))
      )
    )
  )
  result
)

(defun extract-zone (tag / parts)
  (setq parts (str-split tag "-"))
  (nth 1 parts)
)

(defun str-split (str delim / result pos)
  (setq result '())
  (while (setq pos (vl-string-search delim str))
    (setq result (cons (substr str 1 pos) result))
    (setq str (substr str (+ pos 2)))
  )
  (reverse (cons str result))
)

;; ---------------------------------------------------------------
;; 버킷 분포 확인용 유틸 (오늘 학습한 "분포가 중요하다"를 직접 눈으로 확인)
;; ---------------------------------------------------------------
(defun bucket-distribution (buckets / result b)
  (setq result '())
  (foreach b buckets
    (setq result (cons (length b) result))
  )
  (reverse result)
)

(princ "\n[good_hash_lookup.lsp] loaded: build-pipe-hash, find-pipe-hash, getall-by-zone-hash, bucket-distribution")
(princ)
