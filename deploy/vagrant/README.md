# Chaining-Box deployment using Vagrant

This deployment option is intended mainly for testing and development. To start the environment simpy run:

    vagrant up && vagrant reload

The reload is issued just to make sure all VMs are correctly setup. This should only be necessary after running `vagrant up` for the first time (VM creation).

Download and copy [kernel-5.2.0-rc6](https://git.kernel.org/torvalds/t/linux-5.2-rc7.tar.gz) to each host (needed for compilation).

Install dependencies:

    vagrant provision --provision-with=deps

Install code on nodes and start SFs:

    run-nodes.sh
