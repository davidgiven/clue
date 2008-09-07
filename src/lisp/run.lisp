;;; SBCL runtime loader
;;;
;;; © 2008 Peter Maydell.
;;; Clue is licensed under the Revised BSD open source license. To get the
;;; full license text, see the README file.
;;;
;;; $Id$
;;; $HeadURL$
;;; $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

;;; The 'cluerun' script invokes sbcl. The arguments are:
;;;  * the engine name (always 'sbcl')
;;;  * all the input files
;;;  * "--"
;;;  * user arguments, if any

;;; We have to pull in the runtime and C library and then all the input
;;; files. At that point we can run the user's program.

;;; *posix-argv* is our argv array (first element is sbcl's executable name)

;;; We could disable the sbcl debugger here
(write-line "run.lisp starting")

(format t "command line: ~a~%" *posix-argv*)

(load "src/lisp/crt.lisp")
(load "src/lisp/libc.lisp")

(write-line "run.lisp about to load clue")

(defvar _main)

(let (cargs (userargs 
       ;; NB skip the sbcl executable name and the engine name
       (loop for (arg . rest) on (cdr (cdr *posix-argv*))
             while (string/= arg "--")
             do (format t "loading ~a~%" arg)
             do (load arg)
             finally (return rest))))
  
  (setf cargs (map 'list #'clue-newstring userargs))

  (write-line "run.lisp about to run initializers")
  (clue-run-initializers)
  
  (write-line "run.lisp about to call main")
  ;; FIXME we should propagate return value
  (funcall _main 0 (make-array 0 :fill-pointer 0 :adjustable t) 
           (length userargs) 0 cargs)
  (write-line "run.lisp main returned")
 )

(quit)
