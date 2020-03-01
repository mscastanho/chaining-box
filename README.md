# Disaggregated SFC architecture with eBPF

## Dependencies

  - kernel v5.3
  - clang 9.0
  - elfutils 0.176
  - bpftool v5.3.0
  - perf v5.3.g4d856f72c10e
  - pahole v1.15
  - Docker 18.09.7
  - openvswitch 2.9.5
  - Ubuntu 18.04

## Getting the source code

  git clone  --recurse-submodules https://github.com/mscastanho/chaining-box.git

The extra flag is important to correctly initialize the src/libbpf dir, which is
a git submodule.

## Building the Docker container

  cd chaining-box
  docker build -t mscastanho/chainingbox:cb-build .

You can also download it directly from  [DockerHub](https://cloud.docker.com/repository/docker/mscastanho/chainingbox/general).

## Compiling the source code

The best way to compile the source code is to use the pre-built Docker image
containing all build dependencies, so you don't have to install anything (besides
Docker, of course). This is done through `compile.sh` from `src/`.

  cd src/
  ./build.sh

This will compile the source code and place the generated object files and executables
on a `build/` directory under `src/`.

That script basically calls `make` inside the container. You can pass extra arguments to
make directly through the script:

  cd src/
  ./build.sh debug

### Compiling without the Docker container

Make sure to have dependencies listed above and the kernel sources downloaded
somewhere (e.g.: ~/devel/linux) and install the kernel headers locally:

  cd ~/devel/linux
  make headers_install

Now you're ready to compile the code:

    cd chaining-box/src
    make KDIR=~/devel/linux

`KDIR` is needed to point to the updated kernel headers, instead of the ones
offered by the system (which might be outdated).

### Compiling the JITed output

The current build system also supports generating the JITed output for each
program, facilitate program debugging.

    sudo make jited-out KDIR=~/devel/linux-5.3/

where `KDIR` can have a different value. `sudo` is needed since we need to use
bpftool under the hood. The output will be on `jited-output/`

## Running

### cb_docker

  docker image pull mscastanho:chaining-box/cb-node

It generates logs with the output from all the startup processes on each SF
under `/tmp/<sfname>.out`.

### Quick commands
/cb/test/config-cls-docker.sh
ping 172.17.0.3

mount -t debugfs debugfs /sys/kernel/debug
tcpdump -i eth0 -Q in icmp
cat /sys/kernel/debug/tracing/trace_pipe

### Running the tests

#### With Linux bridge
Add the physical interfaces to the Docker bridge

  # brctl addif docker0 enp1s0f0
  # brctl addif docker0 enp1s0f1

#### With OVS
Destroy the OVS bridge if it already exists:

  # ovs-vsctl del-br cbox-br

Add the physical interfaces (connected to the generator) to the bridge:

  # ovs-vsctl add-if cbox-br enp1s0f0
  # ovs-vsctl add-if cbox-br enp1s0f1

Start the containers. E.g.:

  $ cd ~/chaining-box/src/
  $ cfg=../test/chains-config/test-all-funcs.json; sudo ./build/cb_docker -c $cfg \
    -d ../ -n ovs && ./build/cb_manager $cfg

Start the traffic generator:

  # /usr/src/pktgen-19.12.0/app/x86_64-native-linuxapp-gcc/pktgen -l 0-3 \
    -n 4 -- -P -T -m 2.0,3.1 \
    -s 0:/home/mscastanho/chaining-box/scripts/nsh-traffic-1500.pcap

