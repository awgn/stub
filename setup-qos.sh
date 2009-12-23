insmod ./stub.ko master=eth0 stubs=1 debug=1

tc qdisc add dev stub0 root handle 1: htb default 1
tc class add dev stub0 parent 1: classid 1:1 htb rate 1mbit ceil 1mbit

# flush routes tables and rules
ip route flush tab 10  >& /dev/null
ip rule del prio 100   >& /dev/null

# setup interfaces...
ip addr add 172.16.0.25/12 dev stub0
ip link set dev stub0 up

# SO_BINDTODEVICE avoids policy routing. To handle 
# it correctly for each device add a # default gw 
# in the main table... 
ip route add default via 172.16.0.1 dev stub0 metric 1

# add the (policy) rule for each table... 
ip rule add from 172.16.0.25 tab 10 prio 100

# setup table 10 (stub0)
ip route add 172.16.0.0/12 dev stub0 tab 10
ip route add default via 172.16.0.1 dev stub0 tab 10

