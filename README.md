![DreamSourceLab Logo](DSView/icons/dsl_logo.svg)


# DSView 
DSView is a GUI program for supporting various instruments from [DreamSourceLab](http://www.dreamsourcelab.com), including logic analyzers, oscilloscopes, etc. DSView is based on the [sigrok project](https://sigrok.org).

The sigrok project aims at creating a portable, cross-platform, Free/Libre/Open-Source signal analysis software suite that supports various device types (such as logic analyzers, oscilloscopes, multimeters, and more).

# Status

The DSView software is in a usable state and has official tarball releases. However, it is still a work in progress. Some basic functionality is available and working, but other things are always on the TODO list.

# Useful links

- [dreamsourcelab.com](https://www.dreamsourcelab.com)
- [kickstarter.com](https://www.kickstarter.com/projects/dreamsourcelab/dslogic-multifunction-instruments-for-everyone)
- [sigrok.org](https://sigrok.org)

# Important disclaimer
I am not taking credit for the DSView software or the changes made to it in this repository. It has been vibecoded in 100% and I have not written a single line of code for this project. Nevertheless, I am providing this repository as a convenience for those who might be looking for something like this already done by someone else.

# Building on Windows

The Windows build uses the **MinGW-w64** toolchain from [MSYS2](https://www.msys2.org).
MSVC is not supported: `libsigrok4DSL` and the bundled `minizip`/`xlog` code use
POSIX headers (`unistd.h`, `pthread.h`) and GCC extensions, and CMake will stop
with an explanatory error if you try.

### 1. Install MSYS2

Download and run the installer from https://www.msys2.org, then open the
**"MSYS2 MINGW64"** shell (not "MSYS2 MSYS", not "UCRT64" — the commands below
assume MINGW64) and update it:

```sh
pacman -Syu          # close the shell when it asks you to, then reopen it
pacman -Syu
```

### 2. Install the dependencies

```sh
pacman -S --needed git     mingw-w64-x86_64-toolchain     mingw-w64-x86_64-cmake     mingw-w64-x86_64-ninja     mingw-w64-x86_64-pkgconf     mingw-w64-x86_64-qt6-base     mingw-w64-x86_64-qt6-svg     mingw-w64-x86_64-qt6-tools     mingw-w64-x86_64-glib2     mingw-w64-x86_64-zlib     mingw-w64-x86_64-libusb     mingw-w64-x86_64-fftw     mingw-w64-x86_64-boost     mingw-w64-x86_64-python
```

### 3. Build

```sh
git clone https://github.com/DreamSourceLab/DSView
cd DSView
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The executable ends up in `build.dir/DSView.exe`. The build copies `decoders/`,
`res/`, `demo/` and `lang/` next to it, because on Windows DSView looks for its
runtime data in the directory of the executable.

### 4. Run

From the MINGW64 shell the Qt and glib DLLs are already on `PATH`:

```sh
./build.dir/DSView.exe
```

To run it outside MSYS2 (or to hand it to someone else), copy the runtime
libraries next to the executable:

```sh
windeployqt6 --release build.dir/DSView.exe
ldd build.dir/DSView.exe | grep mingw64 | awk '{print $3}' | xargs -I{} cp {} build.dir/
```

To talk to a DSLogic/DSCope device you still need the WinUSB driver bound to it,
which is what the official DSView installer does; [Zadig](https://zadig.akeo.ie)
can do the same for a self built copy.

### Troubleshooting

| Symptom | Cause |
| --- | --- |
| `Compatibility with CMake < 3.5 has been removed` | Old checkout — `cmake_minimum_required` was raised to 3.16 for this reason. |
| `Please install pkg-config!` / `Please install glib!` | You are in the "MSYS2 MSYS" shell instead of "MSYS2 MINGW64", so the `mingw-w64-x86_64-*` packages are not on `PKG_CONFIG_PATH`. |
| `QTextCodec: No such file or directory` | Old checkout — the Windows code paths still used Qt5-only classes. |
| `undefined reference to pv::WinNativeWidget::...` | Old checkout — `winnativewidget.cpp`/`winshadow.cpp` were missing from `CMakeLists.txt`. |

# Copyright and license

DSView software is licensed under the terms of the GNU General Public License
(GPL), version 3 or later.

While some individual source code files are licensed under the GPLv2+, and
some files are licensed under the GPLv3+, this doesn't change the fact that
the program as a whole is licensed under the terms of the GPLv3+ (e.g. also
due to the fact that it links against GPLv3+ libraries).

Please see the individual source files for the full list of copyright holders.
