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
    packet device create --hostname cb02 --plan c3.small.x86 --operating-system ubuntu_18_04 --facility sjc1 --project-id $PACKET_PROJECT

**TODO**: For the final version, we used n2.xlarge.x86 instances through the
spot market. (Actually use n2.xlarge for generator and c3.small for DUT).

**TODO**: parse IP from JSON
This command parses the device data and appends it to your personal
`.ssh/config` file for easy SSH access:

    packet device get --project-id=$PACKET_PROJECT -j | jq -r '.[] | "\nHost \(.hostname)\n\tUser root\n\tHostname \(.ip_addresses[0].address)"' >> ~/.ssh/config

or

    packet device get --id <server-id> -j | jq '.ip_addresses[0].address' | tr '"' '\0'"'

Convert server to Mixed/Hybrid Networking

Make sure you have two VLANs created for your project *on the same DC you
reserved your systems*.

We will use VLANs to isolate TX and RX traffic from/to the traffic generator. For each system:
  - Add eth1 to vnet0
  - Add eth2 to vnet1

Remove interfaces from bond.
  - generator: eno2 and eno4
  - dut: enp1s0f1

    ifenslave -d bond0 enp1s0f1

## Setup the environment

    scp ./install-deps.sh root@cb01:~/ && ssh root@cb01 "~/install-deps.sh"
    scp ./install-deps.sh root@cb02:~/ && ssh root@cb02 "~/install-deps.sh"

Reboot server to load new kernel.

Setup SRIOV:

    echo 6 > /sys/class/net/enp1s0f0/device/sriov_numvfs

Instantiate Chaining-Box environment

    cd chaining-box/src
    cfg=../test/chains-config/len-2-noveth-swap.json; ./build/cb_docker -c $cfg -d ../ -n sriov && ./build/cb_manager $cfg

Configure classifier. *Remember all configuration should be done in terms of
enp1s0f0!!*

    cd chaining-box/src
    ip link set dev enp1s0f0 xdp obj ./build/sfc_classifier_kern.o sec classify
    cd ../deploy/packet
    ./add-cls-rule.sh 10.88.100.131 10.88.100.129 1000 2000 17 100
    ./add-cls-rule.sh 10.88.100.131 10.88.100.129 0 0 1 100

Configure `src_mac` properly

    ./add-srcmac-rule.sh enp1s0f0

Configure the correct srcip to be filtered by the SFs (inside each container):

    docker exec -it sf1 /cb/deploy/packet/add-srcip-rule.sh 192.168.0.1
    ...
    docker exec -it <last-container-in-chain> /cb/deploy/packet/add-srcip-rule.sh 10.88.100.131 swap

**WARNING**: If you forget to add `swap` to the last element in the chain you
risk creating a loop in your chain.

Send packets (from the generator):

   hping3 --udp -k -c 1 -d 100 -s 1000 -p 2000 10.88.100.129

Starting pktgen:

    /usr/src/pktgen-19.12.0/app/x86_64-native-linuxapp-gcc/pktgen -l 10,12,44 -n 4 -- -P -T -m 12.0,44.1


# DUT
  sudo apt install vlan
  sudo modprobe 8021q

  ifenslave -d bond0 enp1s0f1

Edit /etc/network/interfaces
  - remove enp1s0f1 from line of slaves on bond0 setup
  - Replace enp1s0f1 config by:
		auto enp1s0f1.1044
		iface enp1s0f1.1044 inet static
				address 192.168.0.2
				netmask 255.255.255.0
				vlan-raw-device enp1s0f1

		auto enp1s0f1.1267
		iface enp1s0f1.1267 inet static
				address 192.168.1.2
				netmask 255.255.255.0
				vlan-raw-device enp1s0f1

./add-cls-rule.sh 192.168.0.1 192.168.1.1 1000 2000 17 100
./add-cls-rule.sh 192.168.0.1 192.168.1.1 0 0 1 100
# TG
	ifdown eno2 && ifdown eno4
Edit /etc/network/interfaces
	- remove eno2 and eno4 of slaves on bond0 setup
  - Replace their config by:
		auto eno2
		iface eno2 inet static
				address 192.168.0.1
				netmask 255.255.255.0

		auto eno4
		iface eno4 inet static
				address 192.168.1.1
				netmask 255.255.255.0


	 ifup eno2 && ifup eno4
