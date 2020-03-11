# BPF Firewall using libkefir

This function uses libkefir to implement a simple Firewall. This lib takes input
firewall rules and creates a BPF program to implement a filter to enforce those rules. 

## Patched libkefir

To compile this application you will need a patched version of libkefir that is already
provided by the cb-build container. To patch it yourself:

  $ cd libkefir
  $ git am < 0001-Add-function-to-fill-pinned-map.patch
  
## Rule generation

Generate 10 random firewall rules:

  $ ./generate-rules.sh 10 > rules.json

## BPF program generation

Generate the C program based on these rules:

  $ ./generate-firewall rules.json

This will generate a file named firewall_kern.c on this same directory.

## Loading the program

Load the firewall:

  # ./load-firewall.sh eth1 eth2

This last command implicitly fills the map with the created rules, which
can be done separately by:

  # ./fill-map rules.json
