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
  ```bash
  $ wget https://launchpad.net/ubuntu/+archive/primary/+files/libelf_0.8.13.orig.tar.gz
  $ tar -zxvf libelf_0.8.13.orig.tar.gz
  $ cd libelf-0.8.13.orig/
  $ ./configure
  $ make
  $ make install
  ```

+ `m4`:
  ```bash
  $ wget http://ftp.gnu.org/gnu/m4/m4-1.4.18.tar.gz
  $ tar -zxvf m4-1.4.18.tar.gz
  $ cd m4-1.4.18/
  $ ./configure
  $ make
  $ make install
  ```

+ `elfutils`:
  ```bash
  $ wget https://fedorahosted.org/releases/e/l/elfutils/0.161/elfutils-0.161.tar.bz2
  $ tar -xvf elfutils-0.161.tar.bz2
  $ cd elfutils-0.161
  $ ./configure --prefix=$HOME
  $ make
  $ make install
  $ export COMPILER_PATH=/usr/bin
  ```

+ `libdwarf`:
  ```bash
  $ git clone git://libdwarf.git.sourceforge.net/gitroot/libdwarf/libdwarf
  $ cd libdwarf
  $ ./configure --enable-shared
  $ make
  ```

+ [gflags][gflags]
  - Check [this](https://github.com/gflags/gflags/blob/master/INSTALL.md) how-to-install guide.

[gflags]: https://gflags.github.io/gflags/

+ [toml11][toml11]
  ```bash
  $ git clone https://github.com/ToruNiina/toml11.git
  $ ln -s "$(pwd)"/toml11 mcsim_private/toml11
  ```

[toml11]: https://github.com/ToruNiina/toml11


How to compile the simulator?
-----------------------------

1. Download the Pin at [Pin - A Binary Instrumentation Tool](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads).
```bash
$ wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz 
$ tar -xvf pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz 
```

2. Download the McSimA+ simulator at [Scalable Computer Architecture Laboratory](http://scale.snu.ac.kr/). The URL to the repository might be different from the example command below:
```bash
$ git clone https://github.com/scale-snu/mcsim_private.git --recursive
```

3. Create a `Pin` symbolic link in the `mcsim_private` directory.
```bash
$ ln -s "$(pwd)"/pin-3.7-97619-g0d0c92f4f-gcc-linux mcsim_private/pin
```

4. Go to `McSim` and compile McSim. (To build the back-end, the 
  absolute path of `pin` header is required)
```bash
$ cd mcsim_private/McSim
$ mkdir build; cd build
$ cmake ..
$ cmake --build .  -- -j
```

5. Go to `snappy` and run `cmake`.
```bash
$ cd ../../snappy
$ mkdir build; cd build
$ cmake ..
```

6. Go to `Pthread` and compile the user-level thread library pin 
  tool [2] (called `mypthreadtool`) as a dynamic library. (To build the front-end, 
  the absolute path of `pin` root directory should be provided)
```bash
$ cd ../../Pthread
$ make PIN_ROOT="$(pwd)"/../pin obj-intel64/mypthreadtool.so -j4
```

7. Go to `TraceGen` and compile the trace generator pin tool.
```bash
$ cd ../TraceGen
$ make PIN_ROOT="$(pwd)"/../pin obj-intel64/tracegen.so -j
```


How to use the simulator?
-------------------------
There is an example pthread application called `stream` in McSim.  It
is similar to the popular stream benchmark that adds two streams.  To 
run `stream` on top of the McSim framework,

1. Go to McSim/stream and compile stream.
```bash
$ cd ../McSim/stream
$ make
```

2. Add directories containing `pin` and `mypthreadtool` to the
   `$PATH` environment variable.
```bash
$ cd ../../
$ source bash_setup
```

```bash
# 'bash_setup' file is written as follows.
BASE="$(pwd)"
export PIN=${BASE}/pin/pin
export PINTOOL=${BASE}/Pthread/obj-intel64/mypthreadtool.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```

3. Add the absolute path of `stream` directory to `Apps/list/run-stream.toml`
```toml
[[run]]
type = "pintool"
num_threads = 4
num_instrs_to_skip_first = 0
path = "/home/gajh/repository/mcsim_private/McSim/stream"
arg  = "STREAM -p4 -n1048576 -r10 -s512"
```

4. Type the following command:
```bash
$ ./McSim/build/mcsim -mdfile Apps/md/asymmetric-o3-closed.toml -runfile Apps/list/run-stream.toml
```


How to play the trace file
--------------------------
McSimA+ supports trace-driven simulation. `TraceGen` executes the
program using Pin, and extracts proper values corresponding to
instructions and addresses to the snappy file. In order to generate
snappy files and play traces, try the following example:

1. Generate a snappy file extracted form `/bin/ls`.
```
# 'TraceGen' has following options.
-prefix #: the name of the output snappy file
-slice_size # p1 p2 .. : SLICE_SIZE and simulation points to extract (p1 p2 ..)
```

```bash
$ ./pin/pin -t TraceGen/obj-intel64/tracegen.so -prefix test -slice_size 100000 0 -- /bin/ls /
```

2. Generate a runfile for the trace file to `Apps/list` in the following format.
```bash
$ mv test.0.snappy /tmp
$ vim Apps/list/run-trace.py
```

```
# 'run_trace.py' file is written as follows.
# format: 0 (abs. path of trace file) (abs. path of dummy program) (dummy program)
0 /tmp/test.0.snappy /bin/ ls
```

3. To play the trace file, Type the following command:
```bash
$ ./McSim/build/mcsim -mdfile Apps/md/asymmetric-o3-closed.toml -runfile Apps/list/run-trace.toml
```


Setting the configuration of the architecture
---------------------------------------------
You can change the configureation of the architecture you will 
simulate by modifying the `md.toml` file.  It uses [TOML](https://toml.io/en/) language.
Most parameters are named intuitively, but please check the source files to understand
the meaning of parameters that are not clear.

To use asymmetric latency memory mode, see the example file 
`asymmetric-o3-closed.toml` in the path below. 

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
