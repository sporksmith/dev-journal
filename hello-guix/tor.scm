(define-module (tor)
  #:use-module (guix packages)
  #:use-module (guix build utils)
  #:use-module (srfi srfi-1)
  #:use-module (guix git-download)
  #:use-module (guix build-system gnu)
  #:use-module ((guix licenses) #:prefix license:)
  #:use-module (gnu packages autotools)
  #:use-module (gnu packages compression)
  #:use-module (gnu packages m4)
  #:use-module (gnu packages libevent)
  #:use-module (gnu packages tls)
)

(define-public tor
  (package
    (name "tor")
    (version "1.0")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                    (url "https://gitlab.torproject.org/tpo/core/tor.git")
                    (commit "release-0.4.6")))
              (sha256
               (base32
                "029y9iiq8kkbfbk2m3gglgzxm1alpa1vz9wmra58ifabd19ac76n"))))
    (build-system gnu-build-system)
    (synopsis "tor synopsis")
    (description "tor description")
    (home-page "https://www.torproject.org/")
    (native-inputs `(("autoconf", autoconf)
                     ("automake", automake)))
    (inputs `(("libevent", libevent)
              ("openssl", openssl)
              ("zlib", zlib)
              ))
    (arguments
      '(#:configure-flags
         (list "--disable-lzma"
               "--disable-zstd"
               "--disable-asciidoc")
        #:phases
        (modify-phases %standard-phases
          (add-after
            'unpack
            'autogen
            (lambda _ (zero? (system* "sh" "./autogen.sh")))))))
    (license license:bsd-3)))
