;;; Common Lisp C runtime
;;;
;;; © 2008 Peter Maydell.
;;; Clue is licensed under the Revised BSD open source license. To get the
;;; full license text, see the README file.
;;;
;;; $Id$
;;; $HeadURL$
;;; $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

(write-line "crt.lisp running")

(defun clue-newstring (s)
  "Given a Lisp string, create an object usable with clue and return the (offset str)"
  ;; we want to create an array whose elements are the byte values of
  ;; the string's characters
  (list 0 (make-array (+ 1 (length s)) :fill-pointer t :adjustable t 
              :initial-contents (nconc (map 'list #'char-code s) '(0)))))

(defun clue-ptr-to-string (po pd)
  "Given a Clue pointer (offset and base) convert back to Lisp string"
  (map 'string #'code-char (subseq pd po (position 0 pd :start po))))

(defparameter *clue-initializer-list* nil)

(defun clue-add-initializer (f)
  (setf *clue-initializer-list* (nconc *clue-initializer-list* (list f))))

(defun clue-run-initializers ()
  (mapc #'funcall *clue-initializer-list*)
  (setf *clue-initializer-list* nil))

;;; aref doesn't autoextend arrays so we provide a helper fn
(defun clue-set-aref (base off value)
  "Set element off of array base to value, extending array as necessary"
  (if (<= (fill-pointer base) off)
      (adjust-array base (+ 1 off) :fill-pointer t))
  (setf (aref base off) value))

