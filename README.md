# 86x64
Convert 32-bit i386 executables to 64-bit x86\_64 executables.
With macOS 10.15, Apple dropped support for running 32-bit executables under 64-bit macOS.

## Development Status
86x64 is still in a highly experimental stage.
It can currently translate some simple 32-bit programs to 64 bits -- for examples, see the `tests/` directory.

## Getting Started
These instructions will guide you through setting up 86x64 on your Mac.

### Prerequisites
86x64 requires macOS with Command Line Tools installed.

### Installing

86x64 can be installed with the usual `cmake ..; make; make install` combo.

Obtain a copy of the repository and enter it:

```
git clone https://github.com/nmosier/86x64.git && cd 86x64
```

Create a build directory and enter it:

```
mkdir build && cd build
```

Configure the build:

```
cmake ..
```

Build the project;

```
make
```

Install the project:

```
make install
```

By default, 86x64 files will be installed to `/usr/local/opt/86x64`.

## Usage

## Authors
- Nicholas Mosier (nmosier)

## Acknowledgements
- Thanks to michaeljclark for his repository https://github.com/michaeljclark/libSystem-mmap

