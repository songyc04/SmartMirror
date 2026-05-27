#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

const int PORT        = 9000;
const int QT_PORT=9001;
const int BUFFER_SIZE = 1024;
// ── 수신 데이터 파싱 및 출력 ─────────────────────────
// 형식: "TEMP:26.00,HUMI:44.00"
void printParsed(const std::string& data) {
    std::string temp_val = "?";
    std::string humi_val = "?";

    size_t t_pos = data.find("TEMP:");
    size_t h_pos = data.find(",HUMI:");

    if (t_pos != std::string::npos && h_pos != std::string::npos) {
        temp_val = data.substr(t_pos + 5, h_pos - (t_pos + 5));
    }
    if (h_pos != std::string::npos) {
        humi_val = data.substr(h_pos + 6);
        humi_val.erase(humi_val.find_last_not_of(" \n\r\t") + 1);
    }

    std::cout << "┌─────────────────────────┐\n";
    std::cout << "│  온도: " << temp_val << " °C\n";
    std::cout << "│  습도: " << humi_val << " %\n";
    std::cout << "└─────────────────────────┘\n";
}

int main() {
    // 1. 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket 생성 실패");
        return 1;
    }

    // 2. 포트 재사용 허용
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 주소 설정 및 바인딩
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind 실패");
        return 1;
    }

    // 4. 연결 대기
    listen(server_fd, 5);
    //QT 전송용 UDP 소켓 생성
    int qt_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in qt_addr{};
    qt_addr.sin_family = AF_INET;
    qt_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    qt_addr.sin_port = htons(QT_PORT);
    std::cout << "[서버] 포트 " << PORT << " 에서 대기 중...\n";

    // 5. 클라이언트 반복 수락
    // 아두이노가 매 전송마다 연결/종료를 반복하므로 루프로 처리
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int conn_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept 실패");
            continue;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "\n[서버] 연결됨: " << client_ip << "\n";

        // 6. 수신 루프
        char buffer[BUFFER_SIZE];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                std::cout << "[서버] 연결 종료\n";
                break;
            }
            std::string received(buffer);
            std::cout << "[서버] 원본 수신: " << received << "\n";

            // 파싱 출력
            printParsed(received);

            //Qt 프로그램으로 데이터 전송 
            sendto(qt_udp_sock, buffer, bytes, 0,(sockaddr*)&qt_addr, sizeof(qt_addr));
            // 에코 응답 없음 (ESP-01 SEND OK 타이밍 문제 방지)
        }
        close(conn_fd);
    }
    close(qt_udp_sock);
    close(server_fd);
    return 0;
}

