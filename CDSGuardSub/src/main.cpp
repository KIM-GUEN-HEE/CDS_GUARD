#include <iostream>
#include <string>
#include "SendMode.h"
#include "RecvMode.h"

void printUsage()
{
    std::cerr << "Usage:\n"
              << "  SendMode: ./CDSGuard send <L2_iface> <Dst_MAC>\n"
              << "  RecvMode: ./CDSGuard recv <L2_iface>\n\n"
              << "  - <L2_iface>   : 인터페이스 이름 (예: enp0s8) for raw L2 receive\n"
              << "  - <Dst_MAC>    : SendMode에서 사용할 목적지 MAC 문자열 (aa:bb:cc:dd:ee:ff)\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string_view mode = argv[1];

    if (mode == "send")
    {
        std::string dst_mac = argv[3];

        if (argc != 4)
        {
            printUsage();
            return 1;
        }

        std::string l2_iface = argv[2];

        run_send_mode(l2_iface, dst_mac);
    }
    else if (mode == "recv")
    {
        if (argc != 3)
        {
            printUsage();
            return 1;
        }
        std::string l2_iface = argv[2];
        run_recv_mode(l2_iface);
    }
    else
    {
        printUsage();
        return 1;
    }

    return 0;
}
