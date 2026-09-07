;;; ============================================================
;;; bad_linear_search.lsp
;;; 나쁜 예: 배관 태그를 연상 리스트(alist)로 선형 탐색
;;;         + Zone ID만으로 hash를 만드는 흔한 실수
;;; ============================================================
;;;
;;; 배관 태그 형식: "P-<ZoneId>-<PipeId>-<Floor>"
;;;   예: "P-101-A-2FL"
;;;
;;; 문제점:
;;;   1) find-pipe-linear : 도면의 배관 개수만큼 매번 순회 -> O(N)
;;;   2) build-bad-hash / find-pipe-bad-hash :
;;;      hash key로 ZoneId만 사용 -> 같은 Zone에 배관이 많으면
;;;      (실무에서 매우 흔함) 특정 버킷에 배관이 몰려서
;;;      "해시테이블"이라는 이름만 쓰고 사실상 선형 탐색이 됨.
;;; ============================================================

;; 전역 비교 횟수 카운터 (성능 체감을 눈으로 보기 위한 계측용)
(setq *compare-count* 0)

(defun inc-compare ()
  (setq *compare-count* (1+ *compare-count*))
)

;; ---------------------------------------------------------------
;; 1) 순수 선형 탐색 (alist)
;;    pipe-alist 형식: (("P-101-A-2FL" . entity-data) ("P-101-B-2FL" . entity-data) ...)
;; ---------------------------------------------------------------
(defun find-pipe-linear (tag pipe-alist / lst found)
  (setq lst pipe-alist)
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

;; Zone 단위 일괄 조회: 결국 전체 리스트를 처음부터 끝까지 훑음
(defun getall-by-zone-linear (zone-id pipe-alist / lst result tag)
  (setq lst pipe-alist)
  (setq result '())
  (while lst
    (inc-compare)
    (setq tag (car (car lst)))
    ;; 태그에서 zone 부분만 뽑아 비교 (예: "P-101-A-2FL" -> "101")
    (if (= (extract-zone tag) zone-id)
      (setq result (cons (car lst) result))
    )
    (setq lst (cdr lst))
  )
  result
)

;; "P-101-A-2FL" -> "101"
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
;; 2) "해시테이블"이라 부르지만 함정이 있는 버전
;;    hash key = ZoneId 뿐 -> identity(Zone+PipeId+Floor)를 무시.
;;    같은 Zone에 배관이 몰리는 실제 도면에서는 버킷 하나에
;;    수백~수천 개가 쌓여 결국 그 버킷 안에서 선형 탐색을 하게 됨.
;; ---------------------------------------------------------------
(defun bad-hash-zone-only (zone-id num-buckets)
  (rem (atoi zone-id) num-buckets)
)

(defun build-bad-hash (pipe-alist num-buckets / buckets idx tag zone entry)
  ;; buckets: num-buckets 크기의 리스트, 각 원소는 (tag . data)의 리스트
  (setq buckets (make-empty-buckets num-buckets))
  (foreach entry pipe-alist
    (setq tag (car entry))
    (setq zone (extract-zone tag))
    (setq idx (bad-hash-zone-only zone num-buckets))
    (setq buckets (bucket-push buckets idx entry))
  )
  buckets
)

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

;; Zone 하나에 배관이 몰려 있으면 이 버킷 안에서 다시 선형 탐색이 필요해짐
(defun find-pipe-bad-hash (tag buckets num-buckets / zone idx bucket lst found)
  (setq zone (extract-zone tag))
  (setq idx (bad-hash-zone-only zone num-buckets))
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

(princ "\n[bad_linear_search.lsp] loaded: find-pipe-linear, getall-by-zone-linear, build-bad-hash, find-pipe-bad-hash")
(princ)
