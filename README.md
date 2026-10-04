# Slipper (v0.1.0-alpha)
An OpenRC-native C++ package management daemon for Gentoo Linux.

Copyright (c) 2026 Cel-Tech-Serv Pty Ltd. All rights reserved.

## Installation

The Slipper installation process is fully automated. **Do not run the installer as root.** The script handles elevated privileges via `sudo` only when necessary for deployment, keeping the build environment strictly within your user space. The installer will automatically invoke the build script to compile the daemon and client.

1. Make the build and install scripts executable:
   chmod +x install.sh build.sh

2. Execute the installer:
   ./install.sh

## Usage

Slipper is split into a background daemon (`slipperd`) and a lightweight frontend client (`slip`). 

User must be in the wheel group to be able to use slipper without elevated privledges.

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
