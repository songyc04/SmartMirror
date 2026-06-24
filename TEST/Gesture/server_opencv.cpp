import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

TCP_IP = "192.168.0.139"      
ARDUINO_PORT = 9000       # 아두이노 TCP 서버 포트
QT_PORT = 9001            # C++ Qt UDP 포트 (UDP로 변경)

arduino_socket = None     # 아두이노용 TCP 소켓 변수
qt_udp_socket = None      # Qt용 UDP 소켓 변수 (글로벌 생성)

# ── 상태 관리 플래그 ──
arduino_connected = False
initial_scan_done = False
scan_start_time = 0

# ── 감정별 맞춤 노래 맵핑 ──
MUSIC_MAP = {
    "happy": "신나는 댄스곡",
    "sad": "잔잔한 위로의 발라드",
    "angry": "스트레스가 풀리는 록/힙합",
    "surprise": "통통 튀는 팝송",
    "neutral": "마음이 편안해지는 어쿠스틱/재즈",
    "fear": "차분한 클래식",
    "disgust": "기분 전환용 트로피컬 하우스"
}

# [TCP] 아두이노 서버 자동 재연결 로직
def connect_to_arduino():
    global arduino_socket, arduino_connected, initial_scan_done, scan_start_time
    while True:
        try:
            if arduino_socket is None:
                print(f"🔄 아두이노(ESP32) TCP 서버 연결 시도 중 ({TCP_IP}:{ARDUINO_PORT})...")
                arduino_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                # 아두이노가 타임아웃으로 블로킹되지 않도록 설정
                arduino_socket.settimeout(3.0)
                arduino_socket.connect((TCP_IP, ARDUINO_PORT))
                print("✅ 아두이노(ESP32) TCP 서버와 연결 성공!")
                
                arduino_connected = True
                initial_scan_done = False 
                scan_start_time = 0
                break
        except Exception as e:
            print(f"❌ 아두이노 연결 실패 ({e}). 2초 후 재시도합니다.")
            arduino_socket = None
            arduino_connected = False
            time.sleep(2)

# [UDP] C++ Qt용 UDP 소켓 초기화 로직 (UDP는 연결 과정이 필요 없습니다)
def init_qt_udp():
    global qt_udp_socket
    try:
        # SOCK_DGRAM 이 UDP 통신을 뜻합니다.
        qt_udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print(f"✅ C++ Qt UDP 송신 준비 완료 (목적지 -> {TCP_IP}:{QT_PORT})")
    except Exception as e:
        print(f"❌ Qt UDP 소켓 생성 실패: {e}")

# ── [함수] 양쪽 서버(TCP / UDP)에 메시지를 동시 송신하는 로직 ──
def broadcast_tcp_message(message):
    global arduino_socket, qt_udp_socket, arduino_connected
    full_msg = message + "\n"
    encoded_msg = full_msg.encode('utf-8')
    
    # 1. 아두이노 (TCP 전송)
    if arduino_socket is not None:
        try:
            arduino_socket.sendall(encoded_msg)
        except Exception as e:
            print(f"❌ 아두이노 TCP 전송 실패 ({e}). 재연결을 가동합니다.")
            arduino_socket = None
            arduino_connected = False
            threading.Thread(target=connect_to_arduino, daemon=True).start()
            
    # 2. C++ Qt (UDP 전송 - 연결 체크 없이 바로 목적지로 데이터 슛)
    if qt_udp_socket is not None:
        try:
            qt_udp_socket.sendto(encoded_msg, (TCP_IP, QT_PORT))
        except Exception as e:
            print(f"❌ C++ Qt UDP 전송 실패 ({e})")

SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 1.5   

# ── [함수] 최초 1회 감정 스캔 후 노래 재생 명령 전송 ──
def analyze_emotion_and_play(frame):
    try:
        print("\n🎯 [DeepFace] 5초 경과! 사용자의 표정을 분석합니다...")
        small_frame = cv2.resize(frame, (320, 240))
        analysis = DeepFace.analyze(img_path=small_frame, actions=['emotion'], enforce_detection=False)
        
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        emotion = analysis["dominant_emotion"] 
        recommended_song = MUSIC_MAP.get(emotion, "랜덤 추천 음악")
        
        print(f"✨ 분석된 감정: [{emotion}] -> 추천 음악: [{recommended_song}]")

        # 양측 호스트에 결과 전달
        broadcast_tcp_message(f"EMOTION_RESULT:{emotion}")
        broadcast_tcp_message(f"PLAY_SONG:{recommended_song}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

def main():
    # 아두이노는 백그라운드에서 TCP 연결 시도
    threading.Thread(target=connect_to_arduino, daemon=True).start()
    # Qt는 UDP이므로 즉시 소켓 개방
    init_qt_udp()

    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    last_swipe_time = 0
    last_volume_time = 0
    prev_x, prev_y = 0, 0       
    swipe_locked = False        

    start_hold_start_time = 0   
    stop_hold_start_time = 0    
    start_triggered = False     
    stop_triggered = False      

    global arduino_connected, initial_scan_done, scan_start_time

    print(f"\n🚀 [하이브리드 AI 엔진] 가동 완료. 아두이노(TCP) 및 Qt(UDP) 연동 시작...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        
        now = time.time() 

        # ── [연결 이벤트] 아두이노 연결 시 최초 1회 5초 대기 후 표정 인식 ──
        if arduino_connected and not initial_scan_done:
            if scan_start_time == 0:
                scan_start_time = now
                print("\n⏳ [이벤트] 아두이노가 연결되었습니다! 거울을 보고 표정을 지어주세요. 5초 뒤 노래가 추천됩니다...")
            elif now - scan_start_time >= 5.0:
                print("📸 찰칵! 표정 데이터를 캡처했습니다.")
                threading.Thread(target=analyze_emotion_and_play, args=(frame.copy(),), daemon=True).start()
                initial_scan_done = True 

        # ── 제스처 인식 루프 ──
        results = hands.process(rgb_frame) 

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmarks = hand_landmarks.landmark
                
                fingers_open = []
                for tip, pip in [(8, 6), (12, 10), (16, 14), (20, 18)]: 
                    if landmarks[tip].y < landmarks[pip].y:
                        fingers_open.append(True)  
                    else:
                        fingers_open.append(False) 
                open_count = fingers_open.count(True)

                is_open_hand = (open_count >= 3) 
                is_fist_hand = (open_count == 0) 

                if is_open_hand:
                    stop_hold_start_time = 0  
                    stop_triggered = False
                    if start_hold_start_time == 0:
                        start_hold_start_time = now 
                    elif now - start_hold_start_time >= HOLD_REQUIRED_TIME and not start_triggered:
                        print("✋ [유지 감지] 1.5초간 보자기 유지 -> 재생(START)")
                        broadcast_tcp_message("GESTURE:START")
                        start_triggered = True 
                        
                elif is_fist_hand:
                    start_hold_start_time = 0 
                    start_triggered = False
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now 
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("✊ [유지 감지] 1.5초간 주먹 유지 -> 멈춤(STOP)")
                        broadcast_tcp_message("GESTURE:STOP")
                        stop_triggered = True 
                else:
                    start_hold_start_time = 0
                    stop_hold_start_time = 0
                    start_triggered = False
                    stop_triggered = False

                current_x = landmarks[0].x
                current_y = landmarks[0].y

                if prev_x != 0 and prev_y != 0:
                    y_diff = current_y - prev_y 
                    x_diff = current_x - prev_x 

                    if now - last_volume_time > VOLUME_COOLDOWN:
                        if y_diff < -0.08:  
                            print("▲ [볼륨 업] VOLUME_UP")
                            broadcast_tcp_message("GESTURE:VOLUME_UP")
                            last_volume_time = now
                        elif y_diff > 0.08: 
                            print("▼ [볼륨 다운] VOLUME_DOWN")
                            broadcast_tcp_message("GESTURE:VOLUME_DOWN")
                            last_volume_time = now

                    if now - last_swipe_time > SWIPE_COOLDOWN:
                        if not swipe_locked:
                            if x_diff < -0.14:   
                                print("◀ [스와이프] 왼쪽 -> 다음 페이지(NEXT)")
                                broadcast_tcp_message("GESTURE:NEXT")
                                last_swipe_time = now
                                swipe_locked = True 
                            elif x_diff > 0.14:  
                                print("▶ [스와이프] 오른쪽 -> 이전 페이지(PREV)")
                                broadcast_tcp_message("GESTURE:PREV")
                                last_swipe_time = now
                                swipe_locked = True 
                        else:
                            if (x_diff > 0.05 or x_diff < -0.05):
                                swipe_locked = False

                prev_x = current_x
                prev_y = current_y
        else:
            prev_x, prev_y = 0, 0
            swipe_locked = False
            start_hold_start_time = 0
            stop_hold_start_time = 0
            start_triggered = False
            stop_triggered = False

        time.sleep(0.03)

    if arduino_socket:
        arduino_socket.close()
    if qt_udp_socket:
        qt_udp_socket.close()
    cap.release()

if __name__ == "__main__":
    main()
