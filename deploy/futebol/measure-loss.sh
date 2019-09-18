#!/usr/bin/env bash

function totalrx {
    ethtool -S ens3 | grep -e "rx_queue_[0-9]*_xdp_packets" | awk '{ sum += $2 } END { print sum }'
}

function totaltx {
    ethtool -S ens3 | grep -e "tx_queue_[0-9]*_packets" | awk '{ sum += $2 } END { print sum }'
}

rxbef=$(totalrx)
txbef=$(totaltx)

perf stat -B -e 'syscalls:sys_enter_sendto' ./c-loopback ens3 192.168.0.222

rxaft=$(totalrx)
txaft=$(totaltx)

echo -e "Sumarry:\n  Total TX packets: $((rxaft - rxbef))\n  Total RX packets: $((txaft - txbef))\n"
