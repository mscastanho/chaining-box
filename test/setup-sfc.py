#! /usr/bin/env python3

import sys
import argparse
from subprocess import call,Popen

def main(argv):
    parser = argparse.ArgumentParser(prog=argv[0],
        description='Configures environment with SFC code and proper rules')

    parser.add_argument('-n', '--nsfs', metavar='<number-of-SFs>',
        help='Number of SFs in the environment', required=True)

    parser.add_argument('-i', '--install', action='store_const', const=True,
        default=False,help='Compiles BPF code and loads it to VMs')

    parser.add_argument('-c', '--config', action='store_const', const=True,
        default=False,help='Installs SFC rules on VMs')

    args = vars(parser.parse_args())

    install = args['install']
    config = args['config']
    nb_sfs = args['nsfs']

    # No arguments passed, do everything
    if (not install) and (not config):
        install = True
        config = True

    if install:
        call(["./install-code.sh",nb_sfs])

    if config:
        call(["./config-rules.sh",nb_sfs])

if __name__ == "__main__":
    main(sys.argv)
