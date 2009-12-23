insmod ./stub.ko master=eth0 stubs=2 debug=1

# flush routes tables and rules
ip route flush tab 10  >& /dev/null
ip route flush tab 11  >& /dev/null
ip rule del prio 100   >& /dev/null
ip rule del prio 100   >& /dev/null

# setup interfaces...
ip addr add 172.16.0.200/12 dev stub0
ip addr add 172.16.0.201/12 dev stub1
ip link set dev stub0 up
ip link set dev stub1 up

# SO_BINDTODEVICE avoids policy routing. To handle 
# it correctly for each device add a # default gw 
# in the main table... 
ip route add default via 172.16.0.1 dev stub0 metric 1 
ip route add default via 172.16.0.1 dev stub1 metric 2 

# add the (policy) rule for each table... 
ip rule add from 172.16.0.200 tab 10 prio 100
ip rule add from 172.16.0.201 tab 11 prio 100

# setup table 10 (stub0)
ip route add 172.16.0.0/12 dev stub0 tab 10
ip route add default via 172.16.0.1 dev stub0 tab 10

# setup table 11 (stub1)
ip route add 172.16.0.0/12 dev stub1 tab 11
ip route add default via 172.16.0.1 dev stub1 tab 11



