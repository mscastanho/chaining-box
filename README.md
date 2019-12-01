# Disaggregated SFC architecture with eBPF

First of all, you'll need to have Vagrant >= 2.1.0. Please check the
[official page](https://www.vagrantup.com/docs/installat ion/) for instructions.

You will also need `vagrant-libvirt` plugin for Vagrant.  To install it, please
follow the instructions on the project's [GitHub page](https://github.com/vagrant-libvirt/vagrant-libvirt).

Next, install ansible:

    pip3 install ansible

Now you are all setup to start the environment. On the base dir, just run:

    vagrant up

Finally, provision all VMs with the necessary stuff:

    vagrant provision --provision-with ansible

This might take a while, but at the end it will have created the VMs, installed
kernel 4.19 and all other dependencies.

The VMs are automatically provisioned during initialization. If something goes
wrong, or you want to execute the `provision.yml` manually, check
[this](https://docs.ansible.com/ansible/latest/scenario_guides/guide_vagrant.html)
page on the Ansible docs.

To access the vm run

    vagrant ssh

Command to compile bpf code:

    clang -O2 -emit-llvm -c <filename>.c -o - | llc -march=bpf -filetype=obj -o <filename>.o

Load eBPF code to iface:

    sudo ip -force link set dev <interface> xdp obj <filename>.o sec .text

Unload eBPF code:

    sudo ip link set dev <interface> xdp off

## Software versions

  - kernel v5.3
  - clang 9.0
  - elfutils 0.176
  - bpftool v5.3.0
  - perf v5.3.g4d856f72c10e
  - pahole v1.15

## Building the Docker container

   cd chaining-box
   docker build -t mscastanho/chainingbox:cb-build .

You can also download it directly from  [DockerHub](https://cloud.docker.com/repository/docker/mscastanho/chainingbox/general).

## Compiling the source code

The best way to compile the source code is to use the pre-built Docker image
containing all build dependencies, so you don't have to install anything (besides
Docker, of course). This is done through `compile.sh` from `src/`.

    cd src/
    ./compile.sh

This will compile the source code and place the generated object files and executables
on a `build/` directory under `src/`.

That script basically calls `make` inside the container. You can pass extra arguments to
make directly through the script:
 
    cd src/
    ./compile.sh debug

### Compiling without the Docker container

Make sure to have the kernel sources downloaded somewhere (e.g.: ~/devel/linux)
and install the kernel headers locally:

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

## Running loss rate test
On the destination:

    iperf3 -s -p 20000

On the origin:

    cd ~/chaining-box/test
    ./lossrate-test <dest-IP> <local-iface>

## Running `cb_docker`

  docker image pull mscastanho:chaining-box/cb-node
