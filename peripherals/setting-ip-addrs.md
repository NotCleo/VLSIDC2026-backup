My windows laptop is using
IP: 192.168.137.1

Give the board the ip
sudo ip addr add 192.168.137.10/24 dev eth0 #or whatever device youll use

give it the gateway
sudo ip route add default via 192.168.137.1

verify
ip addr show eth0
ip route show


test
ping 192.168.137.1
and 
ping 8.8.8.8


IF something weird like the board is discoverable but pinging google doesnt work
go to your wifi/net provider thingy and restart it.


Password for ssh is root1
