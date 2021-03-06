#!/usr/bin/env bash

apt install --no-install-recommends -y \
    ethtool dwarves iputils-ping tcpdump iperf3 xxd \
    linux-image-5.3.0-70-generic linux-tools-5.3.0-70-generic

# # Install custom iproute2 to include libbpf support (included in v5.11).
#     # Since that version has not been release yet, use the latest git revision.
#     - name: Clone latest iproute2
#       git:
#         repo: https://git.kernel.org/pub/scm/network/iproute2/iproute2-next.git
#         dest: "{{ home }}/iproute2"
#         version: "c7897ec2a68b444b8aecc7aaeed8b80a5eefa7ea"
#       tags: iproute2

#     - name: Install latest iproute2
#       shell: ./configure && make && make install
#       args:
#         chdir: "{{ home }}/iproute2"
#       tags: iproute2
