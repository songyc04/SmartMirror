import sys
import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

# ── 네트워크 설정 (ESP8266 환경에 맞게 원격 IP 설정) ──────────────────────────
# ⚠️ 중요: 아두이노 시리얼 모니터에 찍힌 IP 주소를 정확히 적어주세요!
TCP_IP = "127.0.0.7"      
ARDUINO_PORT = 9000           # ESP8266 TCP 서버 포트
QT_PORT = 9001                # C++ Qt UDP 포트

arduino_socket = None         
qt_udp_socket = None          

# ── 상태 관리 플래그 ──
arduino_connected = False
initial_scan_done = False
scan_start_time = 0

# ── 감정별 맞춤 노래 맵핑 ──
MUSIC_MAP = {
    "HAPPY": "신나는 댄스곡",
    "SAD": "잔잔한 위로의 발라드",
    "ANGRY": "스트레스가 풀리는 록/힙합",
    "SURPRISE": "통통 튀는 팝송",
    "NEUTRAL": "마음이 편안해지는 어쿠스틱/재즈",
    "FEAR": "차분한 클래식",
    "DISGUST": "기분 전환용 트로피컬 하우스"
}

# ── [통신] ESP8266 TCP 서버 자동 재연결 로직 ──
def connect_to_arduino():
    global arduino_socket, arduino_connected, initial_scan_done, scan_start_time
    while True:
        try:
            if arduino_socket is None:
                print(f"🔄 ESP8266 TCP 서버 연결 시도 중 ({TCP_IP}:{ARDUINO_PORT})...", flush=True)
                arduino_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                arduino_socket.settimeout(2.5)  # ESP8266의 응답 특성을 고려해 타임아웃 세팅
                arduino_socket.connect((TCP_IP, ARDUINO_PORT))
                print("✅ ESP8266 TCP 서버와 성공적으로 연결되었습니다!", flush=True)
                
                arduino_connected = True
                initial_scan_done = False 
                scan_start_time = 0
                break
        except Exception as e:
            print(f"❌ ESP8266 연결 실패 ({e}). 2초 후 재시도합니다.", flush=True)
            arduino_socket = None
            arduino_connected = False
            time.sleep(2)

# ── [통신] C++ Qt용 UDP 소켓 초기화 로직 ──
def init_qt_udp():
    global qt_udp_socket
    try:
        qt_udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print(f"✅ C++ Qt UDP 송신 준비 완료 (목적지 -> {TCP_IP}:{QT_PORT})", flush=True)
    except Exception as e:
        print(f"❌ Qt UDP 소켓 생성 실패: {e}", flush=True)

# ── [통신] 양쪽 대상(TCP / UDP)으로 메시지 브로드캐스트 ──
def broadcast_message(message):
    global arduino_socket, qt_udp_socket, arduino_connected
    full_msg = message + "\n"
    encoded_msg = full_msg.encode('utf-8')
    
    # 1. ESP8266 전송 (TCP)
    if arduino_socket is not None:
        try:
            arduino_socket.sendall(encoded_msg)
        except Exception as e:
            print(f"❌ ESP8266 TCP 전송 실패 ({e}). 재연결을 가동합니다.", flush=True)
            arduino_socket = None
            arduino_connected = False
            threading.Thread(target=connect_to_arduino, daemon=True).start()
            
    # 2. C++ Qt 전송 (UDP)
    if qt_udp_socket is not None:
        try:
            qt_udp_socket.sendto(encoded_msg, (TCP_IP, QT_PORT))
        except Exception as e:
            print(f"❌ C++ Qt UDP 전송 실패 ({e})", flush=True)

# ── 제스처 쿨다운 및 타이머 설정 ──
SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 1.5   

# ── [AI 기능] 감정 스캔 및 결과 동시 전송 함수 ──
def analyze_emotion_and_play(frame):
    try:
        print("\n🎯 [DeepFace] 사용자의 표정을 분석합니다...", flush=True)
        small_frame = cv2.resize(frame, (320, 240))
        analysis = DeepFace.analyze(img_path=small_frame, actions=['emotion'], enforce_detection=False)
        
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        dominant_emotion = analysis["dominant_emotion"]
        upper_emotion = dominant_emotion.upper()
        
        recommended_song = MUSIC_MAP.get(upper_emotion, "랜덤 추천 음악")
        print(f"✨ 분석된 감정: [{upper_emotion}] -> 추천 음악: [{recommended_song}]", flush=True)

        broadcast_message(f"EMOTION_RESULT:{upper_emotion}")
        broadcast_message(f"PLAY_SONG:{recommended_song}")

    except Exception as e:
        print(f"PYTHON_ERROR (DeepFace): {str(e)}", flush=True)

def main():
    threading.Thread(target=connect_to_arduino, daemon=True).start()
    init_qt_udp()

    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

    # ── [카메라 해결책] 0번이 안 켜지면 1번, 2번을 순차적으로 강제 탐색하는 방어 코드 ──
    cap = None
    for cam_idx in [0, 1, 2]:
        print(f"📷 카메라 인덱스 [{cam_idx}]번 오픈 시도 중...", flush=True)
        cap = cv2.VideoCapture(cam_idx, cv2.CAP_V4L2)
        if cap.isOpened():
            print(f"✅ 카메라 [{cam_idx}]번 장치를 성공적으로 열었습니다!", flush=True)
            break
        cap.release()

    if not cap or not cap.isOpened():
        print("\n❌ PYTHON_ERROR: 모든 인덱스에서 카메라를 열 수 없습니다.", flush=True)
        print("💡 해결법: 웹캠 USB 선을 뽑았다가 다른 포트에 다시 꽂고 터미널에 'pkill -f smart_mirror_core.py'를 입력해 보세요.", flush=True)
        sys.exit(1)
        
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

    print(f"\n🚀 [통합 AI 엔진] 가동 완료. 스트리밍 시작...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        
        now = time.time() 

        # ── [이벤트] 최초 1회 5초 대기 후 표정 인식 실행 ──
        if arduino_connected and not initial_scan_done:
            if scan_start_time == 0:
                scan_start_time = now
                print("\n⏳ [이벤트] ESP8266 연동 완료! 5초 후 표정을 스캔합니다...", flush=True)
            elif now - scan_start_time >= 5.0:
                print("📸 찰칵! 표정 데이터를 캡처했습니다.", flush=True)
                threading.Thread(target=analyze_emotion_and_play, args=(frame.copy(),), daemon=True).start()
                initial_scan_done = True 

        # ── 실시간 제스처 분석 ──
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
                        print("✋ [유지 감지] 보자기 -> GESTURE:START", flush=True)
                        broadcast_message("GESTURE:START")
                        start_triggered = True 
                        
                elif is_fist_hand:
                    start_hold_start_time = 0 
                    start_triggered = False
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now 
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("✊ [유지 감지] 주먹 -> GESTURE:STOP", flush=True)
                        broadcast_message("GESTURE:STOP")
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
                            print("▲ [볼륨 업] VOLUME_UP", flush=True)
                            broadcast_message("GESTURE:VOLUME_UP")
                            last_volume_time = now
                        elif y_diff > 0.08: 
                            print("▼ [볼륨 다운] VOLUME_DOWN", flush=True)
                            broadcast_message("GESTURE:VOLUME_DOWN")
                            last_volume_time = now

                    if now - last_swipe_time > SWIPE_COOLDOWN:
                        if not swipe_locked:
                            if x_diff < -0.14:   
                                print("◀ [스와이프] 다음 페이지(NEXT)", flush=True)
                                broadcast_message("GESTURE:NEXT")
                                last_swipe_time = now
                                swipe_locked = True 
                            elif x_diff > 0.14:  
                                print("▶ [스와이프] 이전 페이지(PREV)", flush=True)
                                broadcast_message("GESTURE:PREV")
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
