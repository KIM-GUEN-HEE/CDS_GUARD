set -e

echo ">> Compiling TestEncryptSend.cpp …"
g++ TestEncryptSend.cpp -o TestEncryptSend \
    -I ../PacketProcessor/include \
    -I ../PacketTransport/include \
    -L ../PacketProcessor/build \
    -L ../PacketTransport/build \
    -lPacketTransport -lprotocol -lalgorithm -laria_ref -ltransport \
    -pthread

read -p "usage: ./TestEncryptSend 60000, Press Enter to end."