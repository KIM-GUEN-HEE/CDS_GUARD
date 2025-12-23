#!/usr/bin/env python3
import socket

BIND_ADDR = "192.168.2.10"
BIND_PORT = 40000

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((BIND_ADDR, BIND_PORT))
    print(f"Listening on UDP {BIND_ADDR}:{BIND_PORT} …")

    try:
        while True:
            data, addr = sock.recvfrom(65535)
            print(f"Received {len(data)} bytes from {addr}:")
            print(data.decode(errors="replace"))
            print("-" * 40)
    except KeyboardInterrupt:
        print("\nInterrupted by user, exiting.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
