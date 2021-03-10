# Deploy using libvirt and QEMU

## Dependencies

- libguestfs-tools-c
- virt-install
- libvirt
- qemu

## Creating the environment

First, make sure your user belongs to the 'libvirt' group, so it can create VMs,
networks and whatnot in the system scope.

Then simply run:

``` shell
    $ ./create-vms.sh
```

This will:
- Download base Ubuntu 18.04 image
- Install dependencies for chaining-box
- Create networks for VM intercommunication
- Create separate images for each VM
- Copy the current `build` directory from the source tree into the images
- Start the VMs
