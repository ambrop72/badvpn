# NOTE: Must be used with patched nixpkgs:
# https://github.com/ambrop72/nixpkgs/tree/cross-mingw-nss

let
    pkgsFun = import <nixpkgs>;
    
    crossSystem = {
        config = "i686-w64-mingw32";
        arch = "x86";
        libc = "msvcrt";
        platform = {};
        openssl.system = "mingw";
        is64bit = false;
    };
    
    pkgs = pkgsFun {
        inherit crossSystem;
    };
    
in
rec {
    inherit pkgs;
    
    drvs = rec {
        badvpnFunc = import ./badvpn-win32.nix;
        badvpn = pkgs.callPackage badvpnFunc {};
        badvpnDebug = pkgs.callPackage badvpnFunc { debug = true; };
    };
    
    badvpn = drvs.badvpn.crossDrv;
    badvpnDebug = drvs.badvpnDebug.crossDrv;
}
