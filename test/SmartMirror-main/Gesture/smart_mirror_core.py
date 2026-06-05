import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

TCP_IP = "192.168.0.139"      
ARDUINO_PORT = 9000       
QT_PORT = 9001            

arduino_socket = None     
qt_socket = None          

# ── 상태 관리 플래그 ──
arduino_connected = False
initial_scan_done = False
scan_start_time = 0

# ── 감정별 맞춤 노래 맵핑 (원하는 명령어/노래로 수정 가능) ──
MUSIC_MAP = {
    "happy": "신나는 댄스곡",
    "sad": "잔잔한 위로의 발라드",
    "angry": "스트레스가 풀리는 록/힙합",
    "surprise": "통통 튀는 팝송",
    "neutral": "마음이 편안해지는 어쿠스틱/재즈",
    "fear": "차분한 클래식",
    "disgust": "기분 전환용 트로피컬 하우스"
}

def connect_to_arduino():
    global arduino_socket, arduino_connected, initial_scan_done, scan_start_time
    while True:
        try:
            if arduino_socket is None:
                print(f"🔄 아두이노(ESP32) TCP 서버 연결 시도 중 ({TCP_IP}:{ARDUINO_PORT})...")
                arduino_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                arduino_socket.connect((TCP_IP, ARDUINO_PORT))
                print("✅ 아두이노(ESP32) TCP 서버와 연결 성공!")
                
                # 아두이노 연결 성공 시, 최초 1회 얼굴 스캔을 위한 변수 초기화
                arduino_connected = True
                initial_scan_done = False 
                scan_start_time = 0
                break
        except Exception as e:
            print(f"❌ 아두이노 연결 실패 ({e}). 2초 후 재시도합니다.")
            arduino_socket = None
            arduino_connected = False
            time.sleep(2)

def connect_to_qt():
    global qt_socket
    while True:
        try:
            if qt_socket is None:
                print(f"🔄 C++ Qt TCP 서버 연결 시도 중 ({TCP_IP}:{QT_PORT})...")
                qt_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                qt_socket.connect((TCP_IP, QT_PORT))
                print("✅ C++ Qt TCP 서버와 연결 성공!")
                break
        except Exception as e:
            print(f"❌ C++ Qt 연결 실패 ({e}). 2초 후 재시도합니다.")
            qt_socket = None
            time.sleep(2)

def broadcast_tcp_message(message):
    global arduino_socket, qt_socket
    full_msg = message + "\n"
    
    # 파이썬은 NULL 대신 None을 사용합니다.
    if arduino_socket is not None:
        try:
            arduino_socket.sendall(full_msg.encode('utf-8'))
        except Exception as e:
            print(f"❌ 아두이노 TCP 전송 실패 ({e}). 재연결을 가동합니다.")
            arduino_socket = None
            global arduino_connected
            arduino_connected = False
            threading.Thread(target=connect_to_arduino, daemon=True).start()
            
    if qt_socket is not None:
        try:
            qt_socket.sendall(full_msg.encode('utf-8'))
        except Exception as e:
            print(f"❌ C++ Qt TCP 전송 실패 ({e}). 재연결을 가동합니다.")
            qt_socket = None
            threading.Thread(target=connect_to_qt, daemon=True).start()

SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 1.5   

# ── [함수] 감정 스캔 후 노래 재생 명령 전송 ──
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

        # 아두이노/Qt 서버로 감정 결과와 재생할 노래 명령을 동시에 전송
        broadcast_tcp_message(f"EMOTION_RESULT:{emotion}")
        broadcast_tcp_message(f"PLAY_SONG:{recommended_song}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

def main():
    threading.Thread(target=connect_to_arduino, daemon=True).start()
    threading.Thread(target=connect_to_qt, daemon=True).start()

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

    print(f"\n🚀 [듀얼 통신 AI 엔진] 가동 완료. 연결을 기다립니다...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        
        now = time.time() 

        # ── [신규 기능] 아두이노 연결 시 최초 1회 5초 대기 후 표정 인식 ──
        if arduino_connected and not initial_scan_done:
            if scan_start_time == 0:
                scan_start_time = now
                print("\n⏳ [이벤트] 아두이노가 연결되었습니다! 거울을 보고 표정을 지어주세요. 5초 뒤 노래가 추천됩니다...")
            elif now - scan_start_time >= 5.0:
                print("📸 찰칵! 표정 데이터를 캡처했습니다.")
                # 메인 영상이 끊기지 않도록 스레드로 감정 분석 실행
                threading.Thread(target=analyze_emotion_and_play, args=(frame.copy(),), daemon=True).start()
                initial_scan_done = True # 분석을 넘겼으므로 플래그 잠금 (다시 실행 방지)

        # ── [기존 기능] 손가락 제스처 인식 ──
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
                        print("✋ [유지 감지] 1.5초간 보자기 유지 -> 일반 재생(START) 명령 전송")
                        broadcast_tcp_message("GESTURE:START")
                        start_triggered = True 
                        
                elif is_fist_hand:
                    start_hold_start_time = 0 
                    start_triggered = False
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now 
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("✊ [유지 감지] 1.5초간 주먹 유지 -> 멈춤(STOP) 명령 전송")
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
                                print("◀ [스와이프] 왼쪽으로 쓸기 -> 다음 페이지(NEXT)")
                                broadcast_tcp_message("GESTURE:NEXT")
                                last_swipe_time = now
                                swipe_locked = True 
                            elif x_diff > 0.14:  
                                print("▶ [스와이프] 오른쪽으로 쓸기 -> 이전 페이지(PREV)")
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
    if qt_socket:
        qt_socket.close()
    cap.release()

if __name__ == "__main__":
    main()
