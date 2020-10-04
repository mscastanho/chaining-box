#!/bin/bash

set -x
if [ "$4" == "" ]; then
    echo "usage: $0 <ingress> <egress> <match_srcip> <match_dstip> <tunnel_srcip> <tunnel_dstip>"
    echo "creates an ipsec tunnel between two machines"
    exit 1
fi

INGRESS="$1"
EGRESS="$2"
MATCHSRC="$3" 
MATCHDST="$4"
TUNNELSRC="$5"
TUNNELDST="$6"

KEY1=0x`dd if=/dev/urandom count=32 bs=1 2> /dev/null| xxd -p -c 64`
#KEY1=0xd4642fe89cb99887aae3e64a76ce5a1ce94329dc6db9b2f8091c9623609703f9
KEY2=0x`dd if=/dev/urandom count=32 bs=1 2> /dev/null| xxd -p -c 64`
#KEY2=0x3d35d17e587719d648f3abb6324600382a86305dbf920925daeef3904cede355
ID=0x`dd if=/dev/urandom count=4 bs=1 2> /dev/null| xxd -p -c 8`
#ID=0x9f011593

#echo "spdflush; flush;" | sudo setkey -c

# What packets to match?
ip xfrm policy add src $MATCHSRC dst $MATCHDST dir out tmpl src $TUNNELSRC dst $TUNNELDST proto esp reqid $ID mode tunnel

# How to handle those packets?
ip xfrm state add src $TUNNELSRC dst $TUNNELDST proto esp spi $ID reqid $ID mode tunnel auth sha256 $KEY1 enc aes $KEY2

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# We also need to add some redirection code to send the packets to the output
# interface. Let's re-use our tc_redirect code.  
# TODO: Instead, we could just implement another simple BPF prog that redirects
# all packets regardless
# TODO: Here we assume the redirect script is in the same dir as this script.
# This could be improved.
${SCRIPTDIR}/tc_bench01_redirect --ingress $INGRESS --egress $EGRESS --srcip $MATCHSRC
set +x
