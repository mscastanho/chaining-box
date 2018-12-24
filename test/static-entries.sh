# if [ -z "$1" ]; then
#     echo "Usage: $0 <br-name>"
#     exit 1
# fi

# br="$1"

# ports=$(brctl showstp $br | grep ".* ([0-9]*)")

# while read -r p; do
#     id=$( echo $p | awk '{print $2}' )
#     id=${id//[(,)]/} 
#     name="$(echo $p | awk '{print $1}' )"
#     echo "Changing $name (id = $id)"
#     bridge link set dev $name learning off
#     bridge link set dev $name flood off
# done <<< "$ports"

bridge fdb add 00:00:00:00:00:0b dev vnet1 master temp
bridge fdb add 00:00:00:00:00:0c dev vnet3 master temp
bridge fdb add 00:00:00:00:00:0d dev vnet5 master temp
bridge fdb add 00:00:00:00:00:0e dev vnet7 master temp
bridge fdb add 00:00:00:00:00:0a dev vnet9 master temp
bridge fdb add 00:00:00:00:00:10 dev vnet11 master temp
bridge fdb add 00:00:00:00:00:0f dev vnet13 master temp
bridge fdb add a0:36:9f:3d:fc:a0 dev enp10s0f0 master temp
bridge fdb add a0:36:9f:3d:fc:a2 dev enp10s0f2 master temp