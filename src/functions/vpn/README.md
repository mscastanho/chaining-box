# IP xfrm

## What packets to match => policy

  ip xfrm policy add src 10.10.10.2 dst 10.10.10.1 proto 1 dir out tmpl src 130.130.130.2 dst 130.130.130.1 proto esp reqid 0x9f011593 mode tunnel

## How to handle those packets => state

  ip xfrm state add src 130.130.130.2 dst 130.130.130.1 proto esp spi 0x9f011593 \
    reqid 0x9f011593 mode tunnel \
    auth sha256 0xd4642fe89cb99887aae3e64a76ce5a1ce94329dc6db9b2f8091c9623609703f9 \
    enc aes 0x3d35d17e587719d648f3abb6324600382a86305dbf920925daeef3904cede355

Note the tuple specified after `tmpl` in the policy must match the one in the
state we created. This is how the framework matches one with the other.

With these commands, all ICMP packets 10.10.10.2 => 10.10.10.1 will be encrypted
and be tunneled using IPsec with their outer ips as 130.130.130.2 => 130.130.130.1
