{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    devkitnix = {
      url = "github:knarkzel/devkitnix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    devkitnix,
  }: let
    pkgs = import nixpkgs { system = "x86_64-linux"; };
    devkitA64 = devkitnix.packages.x86_64-linux.devkitA64;
  in {
    devShells.x86_64-linux.default = pkgs.mkShell {
      buildInputs = [
        devkitA64
        pkgs.cmake
        pkgs.inetutils
      ];
      env = {
        DEVKITPRO = "${devkitA64}";
      };
    };
  };
}
