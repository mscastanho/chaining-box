# Chaining-Box deployment using Vagrant

This deployment option is intended mainly for testing and development. To start the environment simpy run:

    vagrant up && vagrant reload

The reload is issued just to make sure all VMs are correctly setup. This should only be necessary after running `vagrant up` for the first time (VM creation).

Install dependencies:

    vagrant provision --provision-with=deps

Configure bridge for communication:

    sudo static-entries.sh

Install code on nodes and start SFs:

    run-nodes.sh
