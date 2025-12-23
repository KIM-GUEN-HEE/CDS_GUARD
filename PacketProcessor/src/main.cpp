#include <iostream>
#include <vector>
#include "common/Debug.h"
#include "protocol/ProtocolEngine.h"
#include "protocol/ShiftModule.h"
#include "protocol/PaddingModule.h"
#include "protocol/EncryptionModule.h"
#include "transport/L3SocketTransport.h"
#include "transport/L2SocketTransport.h"

int main() {
    DBG_PRINT("=== PacketProcessor Start ===");

    // protocol start
    ProtocolEngine engine;
    // shift (8byte)
    engine.addModule(std::make_unique<ShiftModule>(8));
    // encryption (ARIA, 32byte)
    std::vector<uint8_t> key(32, 0x00);
    engine.addModule(std::make_unique<EncryptionModule>(key));
    // padding (4byte, 0x00)
    engine.addModule(std::make_unique<PaddingModule>(std::vector<uint8_t>{0,0,0,0}));

    // test payload
    std::vector<uint8_t> testPacket = {
        // Ethernet II header (14byte)
        0xff,0xff,0xff,0xff,0xff,0xff,  // DST MAC: FF:FF:FF:FF:FF:FF
        0x00,0x0c,0x29,0xab,0xcd,0xef,  // SRC MAC: 00:0C:29:AB:CD:EF
        0x08,0x00,                      // EtherType: 0x0800 (IPv4)

        // IPv4 header (20byte)
        0x45,0x00, 0x00,0x3c,           // version/IHL=0x45, total length=60
        0x00,0x00, 0x40,0x00,           // ID=0x0000, Flags+Fragment=0x4000
        0x40,0x06, 0x00,0x00,           // TTL=64, Protocol=6(TCP), Checksum=0x0000(dummy)
        0xc0,0xa8,0x01,0x01,            // SRC IP: 192.168.1.1
        0xc0,0xa8,0x01,0x02,            // DST IP: 192.168.1.2

        // TCP header (20byte)
        0x00,0x50, 0xd9,0x2e,           // SRC Port=80, DST Port=55502
        0x00,0x00,0x00,0x00,            // Seq=0
        0x00,0x00,0x00,0x00,            // Ack=0
        0x50,0x02, 0x72,0x10,           // DataOffset=5, Flags=SYN, Win=0x7210
        0x00,0x00, 0x00,0x00,           // Checksum=0x0000, UrgPtr=0

        // TCP payload: "Test Payload" (12byte)
        0x54,0x65,0x73,0x74, 0x20,0x50,
        0x61,0x79,0x6c,0x6f, 0x61,0x64
    };
    DBG_PRINT("Test packet size: %zu bytes", testPacket.size());

    // encryption
    auto encrypted = engine.encrypt(testPacket);
    DBG_PRINT("Encrypted: %zu bytes", encrypted.size());

    // send l3
    L3SocketTransport l3;
    l3.send(encrypted);

    // receive l2
    L2SocketTransport l2;
    auto received = l2.receive();
    DBG_PRINT("L2 Received: %zu bytes", received.size());

    // decryption
    auto decrypted = engine.decrypt(received);
    DBG_PRINT("Decrypted: %zu bytes", decrypted.size());

    // result
    std::cout << "Final Message (hex):";
    for (auto b : decrypted) {
        std::printf(" %02X", b);
    }
    std::cout << std::endl;

    DBG_PRINT("=== PacketProcessor End ===");
    return 0;
}
