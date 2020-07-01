McSimA+ with Asymmetric Latency Memory Model
============================================
This is the McSimA+ [1] simulator to support the Asymmetric 
Latency Memory model. McSimA+ is a timing simulation 
infrastructure that models x86-based asymmetric manycore 
microarchitectures in detail for both core and uncore 
subsystems, including a full spectrum of asymmetric cores 
from single-threaded to multithreaded and from in-order 
to out-of-order, sophisticated cache hierarchies, coherence 
hardware, on-chip interconnects, memory controllers, and 
main memory.  Jung Ho Ahn started to develop McSimA+ when 
he was at HP Labs.  For further questions and suggestions, 
please contact Jung Ho Ahn at gajh@snu.ac.kr

The released source includes these subdirectories:

+ `McSim`: Contain source codes for timing simulator (Back-end).
+ `Pthread`: Contain source code for functional simulator (Front-end).
+ `Apps`: Contain runfile, mdfile and utilities.


Build requirements
------------------
McSimA+ was tested under the following.

+ OS: Ubuntu 16.04.3 LTS (Kernel 4.4.0)
+ Compiler: gcc version 5.4.0
+ Tool: Intel Pin 3.7

To build the McSimA+ simulator on Linux system first install
the required packages with the following command:

+ `libelf`: 
  ```sh
  $ wget https://launchpad.net/ubuntu/+archive/primary/+files/libelf_0.8.13.orig.tar.gz
  $ tar -zxvf libelf_0.8.13.orig.tar.gz
  $ cd libelf-0.8.13.orig/
  $ ./configure
  $ make
  $ make install
  ```

+ `m4`:
  ```sh
  $ wget http://ftp.gnu.org/gnu/m4/m4-1.4.18.tar.gz
  $ tar -zxvf m4-1.4.18.tar.gz
  $ cd m4-1.4.18/
  $ ./configure
  $ make
  $ make install
  ```

+ `elfutils`:
  ```sh
  $ wget https://fedorahosted.org/releases/e/l/elfutils/0.161/elfutils-0.161.tar.bz2
  $ tar -xvf elfutils-0.161.tar.bz2
  $ cd elfutils-0.161
  $ ./configure --prefix=$HOME
  $ make
  $ make install
  $ export COMPILER_PATH=/usr/bin
  ```

+ `libdwarf`:
  ```sh
  $ git clone git://libdwarf.git.sourceforge.net/gitroot/libdwarf/libdwarf
  $ cd libdwarf
  $ ./configure --enable-shared
  $ make
  ```

+ [gflags][gflags]
  - Check [this](https://github.com/gflags/gflags/blob/master/INSTALL.md) how-to-install guide.

[gflags]: https://gflags.github.io/gflags/

+ [toml11][toml11]
  ```
  git clone https://github.com/ToruNiina/toml11.git
  ln -s toml11 mcsim_private/pin
  ```

[toml11]: https://github.com/ToruNiina/toml11

How to compile the simulator?
-----------------------------

1. Download the Pin at [Pin - A Binary Instrumentation Tool](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads).
```sh
$ wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz 
$ tar -xvf pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz 
```

2. Download the McSimA+ simulator at [Scalable Computer Architecture Laboratory](http://scale.snu.ac.kr/). The URL to the repository might be different from the example command below:
```sh
$ git clone https://github.com/scale-snu/mcsim_private.git 
```

3. Create a `Pin` symbolic link in the `mcsim_private` directory.
```sh
$ ln -s "$(pwd)"/pin-3.7-97619-g0d0c92f4f-gcc-linux mcsim_private/pin
```

4. Go to `McSim` and compile McSim. (To build the back-end, the 
  absolute path of `pin` header is required)
```sh
$ cd mcsim_private/McSim
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .  -- -j
```

5. Go to `Pthread` and compile the user-level thread library pin 
  tool [2] (called `mypthreadtool`) as a dynamic library. (To build the front-end, 
  the absolute path of `pin` root directory should be provided)

```sh
$ cd ../Pthread

$ make PIN_ROOT="$(pwd)"/../pin obj-intel64/mypthreadtool.so -j
```

How to use the simulator?
-------------------------
There is an example pthread application called `stream` in McSim.  It
is similar to the popular stream benchmark that adds two streams.  To 
run `stream` on top of the McSim framework,

1. Go to McSim/stream and compile stream.
```sh
$ cd ../McSim/stream
$ make
```

2. Add directories containing `pin` and `mypthreadtool` to the
   `$PATH` environment variable.

```sh
$ cd ../../
$ source bash_setup

# 'bash_setup' file is written as follows.
BASE="$(pwd)"
export PIN=${BASE}/pin/pin
export PINTOOL=${BASE}/Pthread/obj-intel64/mypthreadtool.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```		

3. Add the absolute path of `stream` directory to `Apps/list/run-stream.py`

```
4 0 /home/djoh0967/mcsim_private/McSim/stream STREAM -p4 -n1048576 -r2 -s512
```

4. Type the following command:

```
$ ./McSim/build/mcsim -mdfile Apps/md/asymmetric-o3-closed.py -runfile Apps/list/run-stream.py
```

Setting the configuration of the architecture
---------------------------------------------
You can change the configureation of the architecture you will 
simulate by modifying the `md.py` file.  Its format is similar to
that of Python, but it is not a python file.  Most parameters are
named intuitively, but please check the source files to understand
the meaning of parameters that are not clear.

To use asymmetric latency memory mode, see the example file 
`asymmetric-o3-closed.py` in the path below. 

```
# default setting

pts.mc.mc_asymmetric_mode  = false
pts.mc.tRCD       = 14
pts.mc.tRAS       = 34
pts.mc.tRP        = 14
...

# Asymmetric Latency Mode
# Usage: set 'pts.mc.mc_asymmetric_mode' to 'true', 
# To set a different latency for each memory controller, see below.
# pts.mc.<# MC>.<param>

pts.mc.mc_asymmetric_mode  = true
pts.mc.0.tRCD       = 14
pts.mc.1.tRCD       = 28	
pts.mc.0.tRAS       = 34
pts.mc.1.tRAS       = 68
pts.mc.0.tRP        = 14
pts.mc.1.tRP        = 28
...
```


References
----------

[1] J. Ahn, S. Li, S. O and N. P. Jouppi, "McSimA+: A Manycore Simulator
    with Application-level+ Simulation and Detailed Microarchitecture
    Modeling," in Proceedings of the IEEE International Symposium on
    Performance Analysis of Systems and Software (ISPASS), Austin, TX,
    USA, April 2013.

[2] H. Pan, K. Asanovic, R. Cohn and C. K. Luk, "Controlling Program
    Execution through Binary Instrumentation," Computer Architecture
    News, vol.33, no.5, 2005.


Frequently Asked Questions (FAQ)
--------------------------------

Q: While compiling McSimA+, I have the following error message. How can I solve it?
```bash
/usr/local/bin/ld: unrecognized option '--hash-style=both' 
/usr/local/bin/ld: use the --help option for usage information
collect2: ld returned 1 exit status
```

A: Type the following command and recompile 'McSimA+'
```bash
$ export COMPILER_PATH=/usr/bin
```
