all: ovs dpdk

ovs_dir = openvswitch-2.13.3
ovs-src: openvswitch-2.13.3
openvswitch-2.13.3:
	wget https://www.openvswitch.org/releases/$@.tar.gz
	tar -xf $@.tar.gz
	rm $@.tar.gz

ovs: ovs-src
	cd $(ovs_dir) && \
	./configure --with-dpdk=../$(dpdk_dir)/build CFLAGS="-Ofast -msse4.2 -mpopcnt"
	make -C $(ovs_dir)

dpdk_dir = dpdk-19.11.6
dpdk-src: dpdk-19.11.6
dpdk-19.11.6:
	wget https://fast.dpdk.org/rel/$@.tar.xz
	tar -xf $@.tar.xz
	mv dpdk-stable-19.11.6 $(dpdk_dir)
	rm $@.tar.xz

dpdk: dpdk-src
	make -C $(dpdk_dir) config T=x86_64-native-linux-gcc
	make -C $(dpdk_dir)

.PHONY: ovs-src dpdk-src dpdk ovs
