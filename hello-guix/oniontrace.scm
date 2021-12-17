(define-module (oniontrace)
  #:use-module (guix gexp)
  #:use-module (guix packages)
  #:use-module (guix git-download)
  #:use-module (guix build-system cmake)
  #:use-module (guix licenses)
  #:use-module (gnu packages glib)
)

(define-public oniontrace
  (package
    (name "oniontrace")
    (version "1.0")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                    (url "https://github.com/shadow/oniontrace.git")
                    (commit "main")))
              (sha256
               (base32
                "14hlxs3hxf48j7plvbvmb2rp333k4q8n7fxy276fbf1rk2iwm05z"))))
    (build-system cmake-build-system)
    (synopsis "oniontrace")
    (description "oniontrace")
    (home-page "https://github.com/shadow/oniontrace")
    (inputs (list glib))
    (arguments
      (list #:configure-flags
            #~(list 
               (string-append "-DCMAKE_EXTRA_INCLUDES=" #$(this-package-input "glib") "/include")
               (string-append "-DCMAKE_EXTRA_LIBRARIES=" #$(this-package-input "glib") "/lib"))))
    (license (non-copyleft "https://github.com/shadow/oniontrace/blob/main/LICENSE.md"))))
