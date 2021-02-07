{ pkgs ? (import <nixpkgs> {}) }:
with pkgs;
rec {
    badvpnFunc = import ./badvpn.nix;
    badvpn = pkgs.callPackage badvpnFunc {};
    badvpnDebug = pkgs.callPackage badvpnFunc { debug = true; };
}
