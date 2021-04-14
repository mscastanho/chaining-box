# Generating cb-node Vagrant base box

Start a VM using the Vagrantfile provided in this directory:

``` shell
$ vagrant up
```

Then package it with:

``` shell
$ export VAGRANT_LIBVIRT_VIRT_SYSPREP_OPERATIONS="defaults,-ssh-userdir,-ssh-hostkeys"
$ vagrant package --output cb-node.box
```

Finally add it to your list of boxes to use it locally:

``` shell
$ vagrant box add cb-node.box --name cb-node
```
