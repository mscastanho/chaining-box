# Deploy to Packet.com

Setup SRIOV

    echo 6 > /sys/class/net/enp1s0f0/device/sriov_numvfs

Instantiate Chaining-Box environment

    cd chaining-box/src
    cfg=../test/chains-config/len-2-noveth-swap.cfg; ./build/cb_docker -c $cfg -d ../ -n sriov && ./build/cb_manager $cfg

Configure classifier:

    cd chaining-box/test
    ./load-bpf ../src/build/sfc_classifier_kern.o bond0 cls
    cd ../deploy/packet
    ./add-cls-rule.sh <generator-ip> <bond0-ip> <sport> <dport> <proto> <spi>
    ./add-srcip-rule.sh <generator-ip> swap
