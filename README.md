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