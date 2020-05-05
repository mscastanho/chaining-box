# Deploy to Packet.com

## Requirements

- Packet.com account
- [Packet CLI](https://github.com/packethost/packet-cli)

## Reserve systems

Setup environment variables with your access info:

    export PACKET_TOKEN=<your access token>
    export PACKET_PROJECT=<your project ID>

Create devices:

    packet device create --hostname cb01 --plan c3.small.x86 --operating-system ubuntu_18_04 --facility sjc1 --project-id $PACKET_PROJECT
    packet device create --hostname cb01 --plan c3.small.x86 --operating-system ubuntu_18_04 --facility sjc1 --project-id $PACKET_PROJECT

**TODO**: parse IP from JSON

## Setup the environment

    scp ./install-deps.sh root@<server-ip>:~/ && ssh root@<server-ip> "~/install-deps.sh"

Reboot server to load new kernel.

Setup SRIOV:

    echo 6 > /sys/class/net/enp1s0f0/device/sriov_numvfs

Instantiate Chaining-Box environment

    cd chaining-box/src
    cfg=../test/chains-config/len-2-noveth-swap.json; ./build/cb_docker -c $cfg -d ../ -n sriov && ./build/cb_manager $cfg

Configure classifier. *Remember all configuration should be done in terms of enp1s0f0!!*

    cd chaining-box/src
    ip link set dev enp1s0f0 xdp obj ./build/sfc_classifier_kern.o sec classify
    cd ../deploy/packet
    ./add-cls-rule.sh 10.88.100.131 10.88.100.129 1000 2000 17 100
    ./add-cls-rule.sh 10.88.100.131 10.88.100.129 0 0 1 100

Configure `src_mac` properly

    ./add-srcmac-rule.sh enp1s0f0

Configure the correct srcip to be filtered by the SFs (inside each container):

    docker exec -it <container> /cb/deploy/packet/add-srcip-rule.sh 10.88.100.131
    ...
    docker exec -it <last-container-in-chain> /cb/deploy/packet/add-srcip-rule.sh 10.88.100.131 swap

**WARNING**: If you forget to add `swap` to the last element in the chain you risk creating a loop in your chain.

Send packets (from the generator):

   hping3 --udp -k -c 1 -d 100 -s 1000 -p 2000 10.88.100.129
