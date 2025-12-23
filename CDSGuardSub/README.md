# CDSGuard

- **Send mode**: localhost(127.0.0.1)에서 <UDP_recv_port>로 설정한 포트로 오는 UDP패킷에 대해서 페이로드 전체를 <Dst_MAC>으로 L2 소켓 통신으로 보냄. 이때, EtherType은 "0x88B5"로 설정됨.

- **Recv mode**: 지정된 네트워크 인터페이스에서 수신된 EtherType "0x88B5"로 설정된 L2 통신에 대해서 복호화 작업을 실시, 복호화해서 얻어낸 IPv4 헤더 정보를 토대로 lo 인터페이스를 통해 전달.

## 빌드 방법 
```bash
    chmod +x build.sh
    ./build.sh
```

## 사용법
```bash
Usage:
  SendMode: ./CDSGuard send <L2_iface> <UDP_recv_port> <Dst_MAC>
  RecvMode: ./CDSGuard recv <L2_iface>

  - <L2_iface>   : 인터페이스 이름 (예: enp0s8) for raw L2 receive
  - <UDP_recv_port> : UDP 포트 (127.0.0.1:<port>) for SendMode
  - <Dst_MAC>    : SendMode에서 사용할 목적지 MAC 문자열 (aa:bb:cc:dd:ee:ff)
```