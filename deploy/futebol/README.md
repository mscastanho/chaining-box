# Deploying ChainingBox to FUTEBOL testbed at UFMG

## Setting up your experimentation tools

TODO: Add info about registering with iMinds and setting up jFed

## Creating the environment

On jFed, click o `Open Local` and select the file `ebpf.rspec` provided in this folder. This will request the allocation of a few specific servers, which might be allocated for someone else at the time. You can choose other available nodes through the GUI or modifying the rspec file directly.

After selecting the file, click `Run` and follow the instructions. If all is fine, your VMs will be alocated in a few minutes.

Create a local temp dir to hold the information for each host:

    mkdir /tmp/futhosts

When all nodes turn green, double-click on each of them and copy the SSH command that shows up at the top of the terminal pop-up window. Paste the contents on a text file on the directory just created (`/tmp/futhosts`) with the name you want for that node (ex: `fut01.txt`). The name of each file (without the extension) will represent the name of this node from now on.

Repeat the process for all nodes allocated (using different names).

Then, on a terminal, run:

    source export-hosts.sh

This command will configure the current terminal to access the nodes without the GUI. This should be done for all terminals you intend to use.

After that, you'll have the following commands at your disposal: `sshfut` and `rsyncfut`. Please read the comments on `export-hosts.sh` for instructions on how to use them.

The nodes need a few adjustments to be ready for testing. For each node, run:

    ./boostrap-host.sh <host> <chaining-box-ldir>

The second argument is the local path to the chaining-box directory, which will be copied to the remote host.

