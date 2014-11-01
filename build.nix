with import <nixpkgs> {};
let
   badvpnLocal = pkgs.badvpn.overrideDerivation (attrs: { src = stdenv.lib.cleanSource ./. ; });
in rec {
   badvpn = badvpnLocal;
   badvpnWithDebug = badvpnLocal.override { debug = true; };
}
