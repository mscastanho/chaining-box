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

# Building the Docker images

This directory contains the recipes for the two Docker images used in
this project:

- `cb-build`: Used to compile the eBPF programs and ChainingBox agents. It
              contains all the dependencies needed for this process.
- `cb-node`: Image to be used by ChainingBox nodes.

There are scripts to create images based on Ubuntu 20.04 and Fedora 33. Using
the Fedora version is encouraged since it already packages more recent versions
of clang, bpftool, libbpf, etc.

Note that pre-built version of these images are available on [DockerHub](https://hub.docker.com/r/mscastanho/chaining-box/tags),
so you should only need to build them if you are making changes to their
contents (or want to build them from scratch for some other reason).

Building `cb-build`:

    docker build -t cb-build:f33 -f Dockerfile.build.f33 .

or:

    docker build -t cb-build:focal -f Dockerfile.build.focal .

Buildinf `cb-node`

    docker build -t cb-node -f Dockerfile.node .
