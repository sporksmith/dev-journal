(define-module (tgen)
  #:use-module (guix packages)
  #:use-module (guix git-download)
  #:use-module (guix build-system cmake)
  #:use-module (guix licenses)
  #:use-module (gnu packages pkg-config)
  #:use-module (gnu packages glib)
  #:use-module (gnu packages graph)
)

(define-public tgen
  (package
    (name "tgen")
    (version "1.0")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                    (url "https://github.com/shadow/tgen.git")
                    (commit "b3bc6c44832b96be5b901f8716e8842d46357bad")))
              (sha256
               (base32
                "03w1zyyprlzylkfw7yxc0jsh1fvzb5b30xjvr43h5x6ja5a5mga8"))))
    (build-system cmake-build-system)
    (synopsis "tgen traffic generator")
    (description "tgen longer description")
    (home-page "https://github.com/shadow/tgen/")
    (native-inputs `(("pkg-config", pkg-config)
                     ("glib", glib)
                     ("igraph", igraph)
                     ))
    (license gpl3+)))
