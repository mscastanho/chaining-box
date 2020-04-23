# Chaining-Box tests

## Benchmarks

### Processing times
- Start namespaces
- Compile kernel programs with stats
  build.sh kernel-stats
- Setup environment with 2 SFs (start_ovs.sh)
- Inside container sf1
  - Find out which are the stats maps
    ip link show eth1
    tc filter show dev eth2 egress
    bpftool prog show
    bpftool map show
    Look for the ones with key = 1B and val = 32B
  - Zero stats
    ./cb/build/map_inspector -s <map-id> -z
- Start ping on send container
  ping -c 1000 -i 0.1 10.10.0.2
- When it's done, check the stats (inside sf1)
  ./cb/build/map_inspector -s <map-id>
