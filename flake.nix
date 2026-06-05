{
  description = "ESP-Miner-NerdQAxePlus dev shell: ESP-IDF for ESP32-S3 + Node.js 22";

  inputs = {
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    nixpkgs.follows = "nixpkgs-esp-dev/nixpkgs";
    flake-utils.follows = "nixpkgs-esp-dev/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgs-esp-dev }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ nixpkgs-esp-dev.overlays.default ];
          config = {
            permittedInsecurePackages = [
              "python3.13-ecdsa-0.19.1"
              "python3.13-ecdsa-0.19.2"
            ];
          };
        };
      in {
        devShells.default = pkgs.mkShell {
          name = "esp-miner-nerdqaxe";
          packages = [
            pkgs.esp-idf-esp32s3
            pkgs.nodejs_22
            pkgs.git
          ];
        };
      });
}
