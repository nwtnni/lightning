# https://fasterthanli.me/series/building-a-rust-service-with-nix/part-10
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      with pkgs; {
        packages = rec {
          default = lightning;

          lightning = stdenv.mkDerivation {
            name = "lightning";
            src = ./.;
            nativeBuildInputs = [
              cmake
              pkg-config
            ];

            installPhase = /* bash */ ''
              mkdir -p "$out/lib"
              local prefix="liblightning"

              for suffix in ".a"; do
                cp "''${prefix}''${suffix}" "$out/lib/''${prefix}''${suffix}"
              done

              mkdir -p "$dev/lib"

              sed -i -e "s|@out@|''${out}|g" ../pkgconfig/*.pc
              sed -i -e "s|@dev@|''${dev}|g" ../pkgconfig/*.pc

              cp -r ../pkgconfig "$dev/lib/"
              cp -r ../inc "''${dev}/include"
            '';

            outputs = [
              "out"
              "dev"
            ];
          };
        };
      }
    );
}
