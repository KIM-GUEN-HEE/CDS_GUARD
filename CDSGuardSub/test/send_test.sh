set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <UDP_recv_port>"
    exit 1
fi

UDP_PORT=$1

echo ">> Compiling TestRawSend.cpp …"
g++ TestRawSend.cpp -o TestRawSend \
    -I ../PacketTransport/include \
    -I ../PacketProcessor/include \
    -L ../PacketTransport/build \
    -L ../PacketProcessor/build \
    -lPacketTransport -lprotocol -lalgorithm -laria_ref -ltransport \
    -pthread

read -p "./CDSGuard send veth1 $UDP_PORT 02:00:00:00:00:02 and enter"

echo ">> Running TestRawSend (sending raw frame via UDP to 127.0.0.1:$UDP_PORT) …"
./TestRawSend $UDP_PORT
