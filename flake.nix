{
  description = "SAPIEN";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/23.05";

    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    # Add the system/architecture you would like to support here. Note that not
    # all packages in the official nixpkgs support all platforms.
    "x86_64-linux" "i686-linux" "aarch64-linux" "x86_64-darwin"
  ] (system: let
    pkgs = import nixpkgs {
      inherit system;
      overlays = [];
    };
  in {
    devShells.default = pkgs.mkShell rec {
      name = "SAPIEN";

      packages = with pkgs; [
        # Development Tools
        llvmPackages_16.clang
        cmake
        cmakeCurses

        # Development time dependencies
        gtest

        # Build time and Run time dependencies
        spdlog
        assimp
        boost
        pinocchio
        grpc
        vulkan-headers
        vulkan-loader
        
        (python3.withPackages (pyPkgs: with pyPkgs; [
          pybind11
          numpy
        ]))
      ];

      # Setting up the environment variables you need during
      # development.
      shellHook = let
        icon = "f121";
      in ''
        export PS1="$(echo -e '\u${icon}') {\[$(tput sgr0)\]\[\033[38;5;228m\]\w\[$(tput sgr0)\]\[\033[38;5;15m\]} (${name}) \\$ \[$(tput sgr0)\]"
      '';
    };

    packages.default = pkgs.callPackage ./default.nix {};
  });
}
