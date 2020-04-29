# Deploy to Packet.com

Setup SRIOV

    echo 6 > /sys/class/net/enp1s0f0/device/sriov_numvfs

Instantiate Chaining-Box environment

    cd chaining-box/src
    cfg=../test/chains-config/len-2-noveth.cfg; ./build/cb_docker -c $cfg -d ../ -n sriov && ./build/cb_manager $cfg


