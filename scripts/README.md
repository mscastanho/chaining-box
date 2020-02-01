# Configuring the system for DPDK

**OBS**: This only needs to be done *once* after the system is booted.

Simply run:

    sudo ./configure-dpdk.sh do

To bind interfaces to default driver again (without DPDK), just run:

    sudo ./configure-dpdk.sh undo

## Configuring the NICs for DPDK

To bind the Intel NIC to DPDK, just run:

    sudo ./configure-intel.sh do

To bind the Netronome SmartNIC to DPDK, run:

    sudo ./configure-netronome.sh do

Both scripts also support the `undo` argument, to put all interfaces back to their default drivers.

## Configuring namespaces

If you want to send packets between interfaces without using DPDK, it is necessary to use network namespaces. To create two namespaces and assign one NIC for each, run:

    ./create-namespaces do

To destroy the namespaces and set the interfaces back to the global one:

    ./create-namespaces undo

## Running the traffic generator

If `pktgen-DPDK` is not installed yet, follow instructions from [pktgen's manual](https://pktgen-dpdk.readthedocs.io/en/latest/getting_started.html).

**!!! - Make sure configure the system for DPDK before following the next steps.**

If pktgen is located at `/usr/src/pktgen-3.5.9`, run the following commands to start the traffic generator:

    cd /usr/src/pktgen-3.5.9
    ./app/x86_64-native-linuxapp-gcc/pktgen -l 0-3 -n 4 -- -T -m 2.0,3.1 -P

This will initialize the generator with two ports (0 and 1) and processors 2 and 3 allocated for ports 0 and 1, respectively.

When initialized, pktgen's CLI will look like this:

![pktgen's CLI](./images/pktgen-print.png)

## Send 1 packet

    cd <ebpflow-nsdi-dir>
    /usr/src/pktgen-3.5.9/app/x86_64-native-linuxapp-gcc/pktgen -l 0-3 -n 4 -- -P -T -m 2.0,3.1 -f ./send-packet.pkt

## pktgen-DPDK cheatsheet

- Set packet size on port 0

        Pktgen:/> set 0 size <pkt-size>

- Set packet count on port <port> to 1000

        Pktgen:/> set <port> count 1000

- Start generating traffic at port 0

        Pktgen:/> start 0

- Stop traffic generation at port 0

        Pktgen:/> stop 0

- Send ARP request on port 0

        Pktgen:/> start 0 arp request

- See port statistics

        Pktgen:/> page stats

- Go back to main page

        Pktgen:/> page main

- See list of all available commands

        Pktgen:/> help

## Setup traffic generator for tests

Some examples require specific traffic profiles or some setup before starting the tests, like filling learningswitch's tables.

Such examples have specific setup scripts, contained in this repo as `setup-<example name>.lua`

To test these, inside pktgen do the following:

    script setup-<example name>.lua
    script <test script>.lua
