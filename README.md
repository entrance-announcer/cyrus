# cyrus

Command-line tool to write audio files to block devices for use with miley.

## Capabilities

- resamples audio
- scales and shifts audio samples to a desired range
- configurable output word size
- checks provided block device for format compatibility with miley.

## Compatibility

This project essentially targets Linux. Although all system-calls employed are POSIX compliant, system files are
directly read for information regarding the mounting and partition schemes of connected block devices. Specifically, "
/etc/mtab" and
"/proc/partitions". The latter of these is only implemented on systems that
implement [procfs](https://en.wikipedia.org/wiki/Procfs), which includes most Unix systems, thereby excluding MacOS (BSD
based).

## Acquiring Cyrus

In the GitHub releases, a pre-built static executable is offered, which can be downloaded and invoked on a modern
version of linux. Additionally, Cyrus can be built from source by following the
[development steps]
(#getting-started-with-development). Binary packages are to come...maybe...this is just one component of one school
project for one class...probably not.

## Getting Started with Development

### System Dependencies

- **C++ Compiler** (tested with gcc 11.2.0)

  A C++20 compiler is required to build this project. Examples include
  [Clang](https://clang.llvm.org/) and [GCC](https://gcc.gnu.org/). These are often already included on most systems.

- **Build System** (tested with [Make](https://www.gnu.org/software/make/) 4.3 and [Ninja](https://ninja-build.org/)
  1.10.2)

  Again, these are often already included on most systems. Nevertheless, a build-system supported by CMake will be
  required. A list of supported CMake generators (for buildsystems) can be
  found [here](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

- **[CMake](https://cmake.org/)** (tested with version 3.21.1)

  CMake is used as the (meta) build-system for the project. This should in It can be installed from your system package
  manager, from [CMake's releases page](https://cmake.org/download/), or even through a conan tool requirement, in your
  conan (next step) profile.

  ```bash
  pacman -S cmake # Arch Linux
  ```

- **[conan](https://conan.io/)** (optional) (tested with version 1.43.2)

  Library dependencies are managed with the [conan](https://conan.io/) package manager. Conan can be used to acquire all
  direct and transitive dependencies for cyrus. First, install conan as a system package:

  ```bash
  pip install conan==1.43.2 # pip
  yay -S conan # Arch Linux
  ```

### Cloning

Clone this repository using any git client. The following commands will clone the repository in your current directory.

```bash
git clone git@github.com:entrance-announcer/cyrus.git  # ssh
git clone https://github.com/entrance-announcer/cyrus.git # http
```

Then, navigate to the cloned project's root directory: `cd cyrus`. All further commands will be provided from here.

### Library Dependencies

Acquiring cyrus's dependencies is vastly simplified and easily reproducible using conan. However, the CMake files are
written such that the libraries can be provided through any package that offers a CMake package config-file - this will
not be covered. Using your desired build directory (for example, *build*) in place of `<build>`, install the library
dependencies:

```bash
conan install . -if=<build> --build=missing
```

A non-default conan profile may be specified using the `-pr` argument. This can be used to add tool requirements,
specify different compilers or compilation flags, or
to [use Ninja](https://docs.conan.io/en/latest/integrations/build_system/ninja.html)
(recommended). Further information regarding conan profiles may be found at
[conan's official Profile documentation](https://docs.conan.io/en/latest/reference/profiles.html).

### Building

Before building, CMake must be run to generate the specified build-system files. Various examples are included below:

```bash
# default 
cmake -B <build> -S .

# default build-system, release build (single-config generators)
cmake -B <build> -S . -D CMAKE_BUILD_TYPE=Release 

# default build-system, statically link standard library
cmake -B <build> -S . -D STATIC_EXEC=ON

# Ninja build-system
cmake -B <build> -S . -G Ninja
```

Now, the build can be performed. The resulting binary will be located in `<build>/bin/`.

```bash
cmake --build <build> 
```

### Installation

Although the cyrus binary may be invoked in the build directory, installation can easily be achieved with CMake. By
default, the installation will be placed in the filesystem's "
add-on" path, as specified by the Filesystem Hierarchy Standard (usually /opt). The installation prefix path can be
easily overriden to another system or user path. Furthermore, both *devel* and *runtime* installation components exist.
The former contains every development component, like CMake modules, headers, etc., while the latter is exclusively the
binary created in [Building Section](#Building).

```shell
# default - install all components in system files (requires sudo)
cmake --install <build> 

# override install prefix & install runtime component only
cmake --install <build> --component cyrus_runtime --prefix my_install_dir
```

To uninstall, simply remove cyrus' root install directory:

```bash
# requires sudo
rm -rf /opt/cyrus
```