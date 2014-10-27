with import <nixpkgs> {};
pkgs.badvpn.overrideDerivation (attrs: { src = stdenv.lib.cleanSource ./. ; })
