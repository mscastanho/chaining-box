# Disaggregated SFC architecture with eBPF

First of all, you'll need to have Vagrant >= 2.1.0. Please check the [official page](https://www.vagrantup.com/docs/installation/) for instructions.

You will also need `vagrant-libvirt` plugin for Vagrant.  To install it, please follow the instructions on the project's [GitHub page](https://github.com/vagrant-libvirt/vagrant-libvirt). 

Next, install ansible:

    pip3 install ansible

Now you are all setup to start the environment. On the base dir, just run:

    vagrant up

Finally, provision all VMs with the necessary stuff:

    vagrant provision --provision-with ansible

This might take a while, but at the end it will have created the VMs, installed kernel 4.19 and all other dependencies.

The VMs are automatically provisioned during initialization. If something goes wrong, or you want to execute the `provision.yml` manually, check [this](https://docs.ansible.com/ansible/latest/scenario_guides/guide_vagrant.html) page on the Ansible docs.

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

## Compiling the source code

Make sure to have the kernel sources downloaded somewhere (e.g.: ~/devel/linux) and install the kernel headers locally:

    cd ~/devel/linux
    make headers_install

Now you're ready to compile the code:

    cd chaining-box/src
    make KDIR=~/devel/linux

`KDIR` is needed to point to the updated kernel headers, instead of the ones offered by the system (which might be outdated).

### Compiling the JITed output

The current build system also supports generating the JITed output for each program, facilitate program debugging.

    sudo make jited-out KDIR=~/devel/linux-5.3/

where `KDIR` can have a different value. `sudo` is needed since we need to use bpftool under the hood. The output will be on `jited-output/`
