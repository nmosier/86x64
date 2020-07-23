# 86x64
Convert 32-bit i386 executables to 64-bit x86\_64 executables.

## Getting Started
These instructions will guide you through setting up 86x64 on your Mac.

## Prerequisites
86x64 requires macOS with Command Line Tools installed.

## Installing

### Brief
86x64 can be installed with the usual `cmake ..; make; make install` combo.

### Detailed
Obtain a copy of the repository and enter it, e.g.

```git clone https://github.com/nmosier/86x64.git && cd 86x64```

Create a build directory and enter it:

```mkdir build && cd build```

Configure the build with

```cmake ..```

Build the project with

```make```

Install the project with

```make install```

By default, 86x64 files will be installed to `/usr/local/opt/86x64`.

