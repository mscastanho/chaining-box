#!/usr/bin/env python

import xml.etree.ElementTree as ET
import sys

if len(sys.argv) != 5:
    print("Usage: {} <ovs-bridge-name> <mac> <vm-in.xml> <vm-out.xml>")
    exit(1)

ovsbridge=sys.argv[1]
macaddr=sys.argv[2]
inputfile=sys.argv[3]
outfile=sys.argv[4]

# Open original file
et = ET.parse(inputfile)
root = et.getroot()

vmname = root.find('name').text

ovsnetxml="""
<interface type='bridge'>
 <mac address='{}'/>
 <target dev='{}'/>
 <source bridge='{}'/>
 <virtualport type='openvswitch'/>
</interface>
""".format(macaddr,vmname,ovsbridge)

new_tag = ET.fromstring(ovsnetxml)

devs = root.find('devices')
devs.append(new_tag)

et.write(outfile)
