# This script adds the following tags to the vhost driver:
#       <host tso4='off' tso6='off' ecn='off' ufo='off'/>
#       <guest tso4='off' tso6='off' ecn='off' ufo='off'/>
#
# These are applied to any vhost interfaces in the VM's 
# definition. Run this after the VM is created, and before
# it is provisioned.

if [ -z $1 ]; then
    echo "Usage: $0 <libvirt-hostname>"
    exit 1
fi

GUEST="$1"
XMLDEF="/tmp/$GUEST.xml"

virsh dumpxml $GUEST > $XMLDEF

regex="<driver name='vhost'.*\/>"

if [ -n `$(grep "$regex" $XMLDEF)` ]; then
    sed -i "s/$regex/<driver name='vhost' queues='8'>\n<host tso4='off' tso6='off' ecn='off' ufo='off'\/>\n<guest tso4='off' tso6='off' ecn='off' ufo='off'\/>\n<\/driver>/g" $XMLDEF

    virsh -q define $XMLDEF && \
    virsh -q shutdown $GUEST && \
    sleep 3 && \
    virsh -q start $GUEST && \
    echo "Updated VM's definition and issued hard reboot"
else
    echo "Could not find empty vhost tag. Nothing done."
fi