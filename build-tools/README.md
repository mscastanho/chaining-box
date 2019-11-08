# Installing BPF development dependencies

The playbook `install-build-deps.yml` provides a quick way to install
all the dependencies needed to work with eBPF. This was used and tested
solely on Ubuntu machines, so for non-Debian based distros you changing
the names of some packages may be needed.

During tests, we used Ubuntu 19.10.

To run the playbook:

    ansible-playbook -i "<MACHINE-IP>," -u <USER> -k -K \
      -e "ansible_python_interpreter=/usr/bin/python3 \
         home=/home/vagrant \
         kernel_version=v5.3 \
         kernel_dir=kernel-{{ kernel_version }}" \
      install-build-deps.yml

Some of the parameters in the playbook are configurable, so you can
change, for example, the kernel version being used to download the
corresponding sources.

Where `<MACHINE-IP>` and `<USER>` should be change to the corresponding
values you are using on the target machine. Aansible will ask for the
password for SSH access before running the playbook.
