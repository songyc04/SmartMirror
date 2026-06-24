# 아두이노 대신해서 클라이언트 테스트할 수 있는 코드
# CONNECT로 Qt랑 연결
# ON 입력하면 3초 간격으로 온습도/AQI/BRI 전송
# OFF 입력하면 서버 연결 종료
# 실행 명령어: python client_test.py (경로 지정 필요)

import socket
import time
import random
import threading
import sys
import os

# 서버 설정
SERVER_IP = "192.168.0.134"
SERVER_PORT = 9000

# 글로벌 상태 제어 변수
client_socket = None
is_connected = False
is_streaming = False  

def auto_sensor_sender():
    """연결 상태이면서 ON 명령이 떨어졌을 때 3초마다 데이터를 송신하는 스레드"""
    global client_socket, is_connected, is_streaming
    
    while True:
        if is_connected and is_streaming and client_socket:
            try:
                temp = random.uniform(20.00, 28.00)
                humi = random.uniform(40.00, 60.00)
                aqi = random.randint(1, 5)
                bri = random.randint(100, 300)
                
                sensor_data = f"TEMP:{temp:.2f},HUMI:{humi:.2f},AQI:{aqi},BRI:{bri}\n"
                
                client_socket.sendall(sensor_data.encode('utf-8'))
                print(f"\n[자동 송신] {sensor_data.strip()}")
                print(">> ", end="", flush=True)
                
            except (socket.error, BrokenPipeError):
                print("\n[알림] 서버와의 연결이 끊어졌습니다. 송신을 중단합니다.")
                is_connected = False
                is_streaming = False
                client_socket = None
                print(">> ", end="", flush=True)
                
        time.sleep(3)

def main():
    global client_socket, is_connected, is_streaming
    
    # 주기적 송신 스레드 가동
    sender_thread = threading.Thread(target=auto_sensor_sender, daemon=True)
    sender_thread.start()
    
    print("==================================================")
    print("🤖 아두이노 대체 Qt 테스트용 TCP 클라이언트 시뮬레이터")
    print(f"📡 타겟 서버 - IP: {SERVER_IP}, PORT: {SERVER_PORT}")
    print("📌 [명령어 가이드]")
    print("   - CONNECT : 서버에 연결 요청")
    print("   - ON      : 'ON' 문자열 송신 후, 3초 주기 자동 센서 데이터 송신 시작")
    print("   - OFF     : 'OFF' 문자열 송신 후 정상 연결 해제")
    print("   - END     : 프로그램 즉시 최종 종료")
    print("   - 일반문자열: 연결된 상태에서 입력 시 즉시 서버로 송신")
    print("==================================================")
    
    while True:
        try:
            user_input = input(">> ").strip()
            if not user_input:
                continue
            
            # END 처리
            if user_input == "END":
                print("\n[종료] END 명령어가 입력되었습니다. 프로그램을 완전히 종료합니다.")
                if client_socket:
                    try:
                        client_socket.close()
                    except:
                        pass
                os._exit(0)
            
            # CONNECT 처리
            elif user_input == "CONNECT":
                if is_connected:
                    print("[안내] 이미 서버에 연결되어 있습니다.")
                    continue
                
                print(f"[시도] {SERVER_IP}:{SERVER_PORT} 서버 연결 시도 중...")
                try:
                    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    client_socket.settimeout(4.0) 
                    client_socket.connect((SERVER_IP, SERVER_PORT))
                    client_socket.settimeout(None) 
                    
                    is_connected = True
                    print(f"🤝 [연결 성공] {SERVER_IP}:{SERVER_PORT} 와 연결되었습니다!")
                    print("[안내] 자동 송신을 시작하려면 'ON'을 입력하세요.")
                except Exception as e:
                    print(f"❌ [연결 실패] 서버를 찾을 수 없습니다. ({e})")
                    client_socket = None
                continue
            
            # ✨ ON 처리 (수정됨: ON 문자열 전송 기능 추가)
            elif user_input == "ON":
                if is_connected and client_socket:
                    if is_streaming:
                        print("[안내] 이미 자동 송신이 활성화되어 있습니다.")
                    else:
                        try:
                            # 1. 서버로 "ON" 메시지 먼저 송신
                            print("[송신] 'ON' 신호를 서버로 전송합니다.")
                            client_socket.sendall("ON\n".encode('utf-8'))
                            
                            # 2. 내부 플래그 활성화하여 3초 주기 자동 송신 시작
                            is_streaming = True
                            print("🚀 [송신 시작] 이제부터 3초 간격으로 센서 난수 데이터를 송신합니다.")
                        except (socket.error, BrokenPipeError):
                            print("❌ [오류] 'ON' 신호 송신 실패! 서버 연결을 확인하세요.")
                else:
                    print("[경고] 서버와 연결되지 않았습니다. 'CONNECT'를 먼저 입력하세요.")
                continue
            
            # OFF 처리
            elif user_input == "OFF":
                is_streaming = False 
                if is_connected and client_socket:
                    try:
                        print("[송신] 'OFF' 신호를 전송하고 연결을 안전하게 종료합니다.")
                        client_socket.sendall("OFF\n".encode('utf-8'))
                        time.sleep(0.5) 
                    except socket.error:
                        pass
                    finally:
                        client_socket.close()
                        client_socket = None
                        is_connected = False
                        print("[해제] 서버 연결이 정상적으로 끊어졌습니다.")
                else:
                    print("[안내] 현재 연결된 서버가 없습니다.")
                continue
            
            # 일반 수동 문자열 송신
            else:
                if is_connected and client_socket:
                    try:
                        send_msg = user_input + "\n"
                        client_socket.sendall(send_msg.encode('utf-8'))
                        print(f"[수동 송신 완료] {user_input}")
                    except (socket.error, BrokenPipeError):
                        print("❌ [오류] 송신 실패! 서버와 연결이 끊어졌습니다.")
                        is_connected = False
                        is_streaming = False
                        client_socket = None
                else:
                    print("[경고] 서버와 연결되지 않은 상태입니다. 'CONNECT'를 먼저 입력하세요.")
                    
        except (KeyboardInterrupt, SystemExit):
            print("\n[안내] 프로그램을 종료하려면 'END'를 입력하세요.")
            print(">> ", end="", flush=True)

if __name__ == "__main__":
    main()