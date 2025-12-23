# 테스트코드

## 테스트 환경 및 변경 시 수정해야할 항목
send와 recv의 enp0s3라는 동일명 인터페이스에에 연결된 네트워크는 서로 다른 네트워크 망.
### CDSGuard send 모드 환경
interface: enp0s3
192.168.1.10/24

interface: enp0s8
192.168.254.2/24 <- L2 통신을 위해 연결 후 IP 배정은 했지만, 사용하지 않음. 
(02:00:00:00:00:20)

### CDSGuard recv 모드 환경
interface: enp0s3
192.168.2.10/24

interface: enp0s8
192.168.254.3/24 <- L2 통신을 위해 연결 후 IP 배정은 했지만, 사용하지 않음. 
(02:00:00:00:00:30)

## 테스트 코드 빌드
```bash
chmod +x recv_test.sh
./recv_test.sh
```

## 테스트 방법 
1. CDSGuard send PC에서 아래 명령어 실행
```bash
cd ../build/
sudo ./CDSGuard send enp0s8 60000 02:00:00:00:00:30 
```
2. CDSGuard recv PC에서 아래 명령어 실행
```bash
cd ../build/
sudo ./CDSGuard recv enp0s8 
```
3. CDSGuard recv PC에서 테스트 패킷 수신을 위한 listener 실행
```bash
python3 ./udp_listener.py
```
4. CDSGuard send PC에서 테스트 패킷 전송 실행
```bash
./TestEncryptSend 60000
```
5. 각 터미널에서 다음과 같은 결과를 확인하면 테스트 완료.
sudo ./CDSGuard send enp0s8 60000 02:00:00:00:00:30
L3: Invalid destination IP: 
[*] SEND MODE: Listening on UDP 127.0.0.1:60000 for encrypted L2 payloads...
[<] SEND MODE: Received 73 bytes of encrypted payload from UDP 127.0.0.1:60000
[>] SEND MODE: Forwarded 87 bytes (Ethernet frame) over L2 on enp0s8 (DstMAC=02:00:00:00:00:30)

./TestEncryptSend 60000
[TestEncryptSend] Plain frame length = 69 bytes
[TestEncryptSend] Cipher frame length = 73 bytes
[>] Sent 73 bytes of encrypted frame to 127.0.0.1:60000

sudo ./CDSGuard recv enp0s8
[*] RECV MODE: Listening on interface enp0s8 for EtherType=0x88b5
[>] RECV MODE: Injected 55 bytes of IP packet (via lo)

python3 ./udp_listener.py 
Listening on UDP 192.168.2.10:40000 …
Received 27 bytes from ('192.168.1.10', 40000):
Hello, ENCRYPT!, test, test
----------------------------------------
