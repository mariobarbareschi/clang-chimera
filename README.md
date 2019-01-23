# Clang-Chimera
[![Build Status](https://travis-ci.org/andreaaletto/clang-chimera.svg?branch=master)](https://travis-ci.org/andreaaletto/clang-chimera)
------------
Clang-Chimera is a framework to easily develop operators to automatically modify C/C++ codes through code patterns identification.

See the [guide](doc/guide/guide.pdf)!

## Build clang-chimera from source

In order to build **clang-chimera** from source you will need an installation of LLVM/Clang 3.9.1 compiled with the following cmake flags:
```
 -DLLVM_ENABLE_CXX1Y=true 
 -DLLVM_ENABLE_RTTI=ON
 -DLLVM_ENABLE_FFI=ON 
```

Clang-chimera will expect LLVM/Clang binaries to be installed in ```/usr/bin```.

#### Install LLVM/Clang
------------
Use this section to understand how to install LLVM/Clang 3.9.1 correctly. Note that you will need these packages to be installed:
* cmake
* subversion
* ninja-build
* build-essential (gcc, g++ and their libraries)
* zlib
* ffi
* edit
* ncurses
* boost

Considering Ubuntu  18.04 as development platform, use the following command to install the previous dependencies:
```
$ apt-get install cmake subversion ninja-build build-essential zlib1g-dev libffi-dev libedit-dev libncurses5-dev libboost-dev
```
Now, you can download LLVM/Clang 3.9.1 from source and build it.
```
$ svn co -q http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_391/final llvm
$ cd llvm/tools
$ svn co -q http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_391/final clang
$ mkdir ../build && cd ../build
$ CC=gcc CXX=g++ cmake -DLLVM_ENABLE_CXX1Y=true -DLLVM_ENABLE_RTTI=ON -DLLVM_TARGET_ARCH="X86" -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_ENABLE_FFI=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -G Ninja ..
$ ninja && sudo ninja install
```

When the installation process ends, try to run:
```
$ llvm-config --version
$ clang --version
```

#### Install Clang-Chimera
------------
First of all you need to solve a LLVM bug, involving the JIT-compiler: the file ```/usr/lib/cmake/llvm/LLVM-Config.cmake``` tries to find the JIT-compiler library with the name ***jit***, but it fails because the actual name of this component is ***mcjit***. So it is necessary to edit ```LLVM-Config.cmake``` in this way:
``` 
# sed -i 's/list(APPEND link_components "jit")/list(APPEND link_components "mcjit")/g' /usr/lib/cmake/llvm/LLVM-Config.cmake
``` 
Before building Clang-Chimera, you need to configure it with CMake. You need to pass to CMake configurator the list of LLVM's available components. You can run the script ```run_cmake``` or configure Clang-Chimera manually. 
Now you can build Clang-Chimera source. It is recomended to use [Ninja](https://ninja-build.org/) as building tool, since it takes advantage of the multicore parallelism by default. 
``` 
$ cd ~/clang-chimera 
$ ./run_cmake
$ cd build
$ ninja
$ sudo ninja install
``` 
At the end of the process you will find clang-chimera in ```/usr/bin```. Try run:
``` 
$ clang-chimera -version
``` 

## Quick Start
If you don't want to build LLVM/Clang and Clang-Chimera from scratch, a ready-to-use solution is provided through a [Docker](https://www.docker.com/) Container. Please refer to [IIDEAA Docker](https://github.com/andreaaletto/iideaa-docker) repository for further details.

#### LICENSE
--------

* [GPLV3.0](https://www.gnu.org/licenses/licenses.html)

#### Contributing
----------

Github is for social coding: if you want to write code, I encourage contributions through pull requests from forks of this repository. 
