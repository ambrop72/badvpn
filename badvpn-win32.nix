{ stdenv, cmake, pkgconfig, openssl, nspr, nss, debug ? false }:
let
    compileFlags = "-O3 ${stdenv.lib.optionalString (!debug) "-DNDEBUG"}";
in
stdenv.mkDerivation {
    name = "badvpn";
    
    src = stdenv.lib.cleanSource ./.;
    
    nativeBuildInputs = [ cmake pkgconfig ];
    buildInputs = [ openssl nspr nss ];
    
    NIX_CFLAGS_COMPILE = "-I${nspr.crossDrv}/include/nspr -I${nss.crossDrv}/include/nss";
    
    preConfigure = ''
        cmakeFlagsArray=( "-DCMAKE_BUILD_TYPE=" "-DCMAKE_C_FLAGS=${compileFlags}" "-DCMAKE_SYSTEM_NAME=Windows" );
    '';
    
    postInstall = ''
        for lib in eay32; do
            cp ${openssl.crossDrv}/bin/lib$lib.dll $out/bin/
        done
        for lib in nspr4 plc4 plds4; do
            cp ${nspr.crossDrv}/lib/lib$lib.dll $out/bin/
        done
        for lib in nss3 nssutil3 smime3 ssl3; do
            cp ${nss.crossDrv}/lib/$lib.dll $out/bin/
        done
    '';
}
