;;; Common Lisp C library functions
;;;
;;; © 2008 Peter Maydell.
;;; Clue is licensed under the Revised BSD open source license. To get the
;;; full license text, see the README file.
;;;
;;; $Id$
;;; $HeadURL$
;;; $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

(write-line "libc.lisp running")

;; A utility macro for defining functions which match the API
;; the Clue C compiler uses.
(defmacro defcfn (name args &rest body) "Define a function in the C library"
   `(defparameter ,name (lambda (stackpo stackpd ,@args)
                          (declare (ignore stackpo stackpd))
                          ,@body)))

;;; MEMORY

(defcfn _malloc (ln)
  (declare (ignore ln))
  (list 0 (make-array 0 :adjustable t :fill-pointer t)))

(defcfn _calloc (s1 s2)
  (list 0 (make-array (* s1 s2) :initial-element 0 :adjustable t :fill-pointer t)))

;;; STRINGS

(defcfn _strcpy (doff dd soff sd)
  (list doff (replace dd sd :start1 doff :start2 soff :end2 (position 0 sd :start soff))))

(defcfn _memset (doff dd c n)
  (list doff (fill dd c :start doff :end (+ doff n))))

;;; STDIO

(defcfn _printf (formatpo formatpd)
  (let ((fmt (clue-ptr-to-string formatpo formatpd)))
    ;; FIXME just prints the format string!
    (format t "~a" fmt)))

; putc
; atoi
; atol

;;; TIME

; gettimeofday

;;; MATHS

; sin cos atan log exp sqrt pow
