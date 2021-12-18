(define-module (shadow)
  #:use-module (guix packages)
  #:use-module (guix git-download)
  #:use-module (guix build-system cmake)
  #:use-module (guix licenses)
  #:use-module (guix gexp)
  #:use-module (gnu packages pkg-config)
  #:use-module (gnu packages glib)
  #:use-module (gnu packages graph)
  #:use-module (gnu packages linux)
  #:use-module (gnu packages rust)
)

(define-public shadow
  (package
    (name "shadow")
    (version "1.0")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                    (url "https://github.com/sporksmith/shadow")
                    (commit "20f9961c35c0807f7a6653babdd5dd93ac236e99")))
              (sha256
               (base32
                "0z9m11wirbj8pww9c4y3wyll0iiw4r0l5hnf30mkkwkna7zj80ax"))))
    (build-system cmake-build-system)
    (synopsis "shadow")
    (description "shadow")
    (home-page "https://github.com/shadow/shadow/")
    (inputs (list glib procps))
    (native-inputs `(("rust" ,rust)
                     ("rust:cargo" ,rust "cargo")))
    (arguments
      (list #:configure-flags
            #~(list 
               "-DCMAKE_BUILD_TYPE=Release"
               (string-append "-DCMAKE_EXTRA_INCLUDES=" #$glib "/include")
               (string-append "-DCMAKE_EXTRA_LIBRARIES=" #$glib "/lib"))))
    (license (non-copyleft "https://github.com/shadow/shadow/blob/main/LICENSE.md"))))

shadow
