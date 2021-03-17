# External tools used in the project

Tools:
- DPDK 19.11.6
- Open vSwitch 2.13.3

## Dependencies

Please check the list of packages needed to build these projects in the
corresponding documentation:

- [OVS build requirements](https://docs.openvswitch.org/en/latest/intro/install/general/#build-requirements)
- [DPDK system requirements](https://doc.dpdk.org/guides-19.11/linux_gsg/sys_reqs.html)

## Building everything

The Makefile provided in this directory will download the tools sources and
build (without installing them) for you.

    make
