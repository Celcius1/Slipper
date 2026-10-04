# Slipper (v0.1.2-alpha)

A C++ package management daemon for Gentoo Linux.

Copyright (c) 2026 Cel-Tech-Serv Pty Ltd. Licensed under the GNU General Public License v2.0 (GPL-2.0).

## Installation

Slipper uses a standard CMake build system. The provided build script runs entirely in user-space, and elevated privileges (`sudo` or `doas`) are only required for the final installation step.

1. Make the build script executable and compile the daemon and client:

chmod +x build.sh
./build.sh

2. Install the binaries to your system (respects standard Linux prefixes):

cd build

sudo make install  or  doas make install

3. Add to openRC

## Enable the Service

sudo rc-update add slipperd default

## Start the Service

sudo rc-service slipperd start

## Usage

Slipper is split into a background privileged daemon (`slipperd`) and a lightweight frontend client (`slip`).

Users must be in the `wheel` group to connect to the Slipper daemon via the IPC socket without elevated privileges.

### Merging Packages (Slip In)

Install specific packages or upgrade the system:

slip --in -avs sl

slip --in -uD @world


### Unmerging Packages (Slip Out)

Remove installed packages from the system:

slip --out -avs sl


### Common Flags

* `-a` : Ask for confirmation before proceeding.
* `-v` : Verbose output.
* `-s` : Stream daemon output to console instead of remaining silent.
* `-u` : Update (Enforces resolution of the highest available versions).
* `-D` : Deep (Traverses the full dependency tree).
