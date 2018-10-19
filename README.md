# Disaggregated SFC architecture with eBPF

First of all, you'll need to have Vagrant installed:

    sudo apt install -y vagrant

Install ansible:

    pip3 install ansible


On the base dir, just run

    vagrant up

This might take a while, but at the end it will have created the VM, installed kernel 4.18 and all other dependencies.

The VMs are automatically provisioned during initialization. If something goes wrong, or you want to execute the `provision.yml` manually, check [this](https://docs.ansible.com/ansible/latest/scenario_guides/guide_vagrant.html) page on the Ansible docs.

To access the vm run

    vagrant ssh