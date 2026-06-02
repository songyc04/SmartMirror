import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

TCP_IP = "192.168.0.139"      
ARDUINO_PORT = 9000       # 아두이노 TCP 서버 포트 (9000)
QT_PORT = 9001            # C++ Qt TCP 서버 포트 (9001)

arduino_socket = None     # 아두이노용 소켓 전역 변수
qt_socket = None          # Qt용 소켓 전역 변수

# ── [함수] 아두이노 서버 자동 재연결 로직 ──
def connect_to_arduino():
    global arduino_socket
    while True:
        try:
            if arduino_socket is None:
                print(f"🔄 아두이노(ESP32) TCP 서버 연결 시도 중 ({TCP_IP}:{ARDUINO_PORT})...")
                arduino_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                arduino_socket.connect((TCP_IP, ARDUINO_PORT))
                print("✅ 아두이노(ESP32) TCP 서버와 연결 성공!")
                break
        except Exception as e:
            print(f"❌ 아두이노 연결 실패 ({e}). 2초 후 재시도합니다.")
            arduino_socket = None
            time.sleep(2)

# ── [함수] C++ Qt 서버 자동 재연결 로직 ──
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

# ── [함수] 양쪽 서버 모두에게 메시지를 동시 송신하는 로직 ──
def broadcast_tcp_message(message):
    global arduino_socket, qt_socket
    full_msg = message + "\n"
    
    # 1. 아두이노(9000) 전송 시도
    if arduino_socket is not NULL:
        try:
            arduino_socket.sendall(full_msg.encode('utf-8'))
        except Exception as e:
            print(f"❌ 아두이노 TCP 전송 실패 ({e}). 재연결을 가동합니다.")
            arduino_socket = None
            threading.Thread(target=connect_to_arduino, daemon=True).start()
            
    # 2. C++ Qt(9001) 전송 시도
    if qt_socket is not NULL:
        try:
            qt_socket.sendall(full_msg.encode('utf-8'))
        except Exception as e:
            print(f"❌ C++ Qt TCP 전송 실패 ({e}). 재연결을 가동합니다.")
            qt_socket = None
            threading.Thread(target=connect_to_qt, daemon=True).start()

SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 1.5   

def analyze_emotion_async(frame):
    try:
        print("\n🎯 [DeepFace] 노래 추천을 위한 표정 분석 시작...")
        small_frame = cv2.resize(frame, (320, 240))
        analysis = DeepFace.analyze(img_path=small_frame, actions=['emotion'], enforce_detection=False)
        
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        emotion = analysis["dominant_emotion"] 
        print(f"✨ 분석된 감정 상태: [{emotion}]")

        # 감정 분석 결과도 아두이노와 Qt 양측에 개행문자를 포함해 일괄 전송
        broadcast_tcp_message(f"EMOTION_RESULT:{emotion}")
        print(f"[TCP 브로드캐스트 완료] EMOTION_RESULT:{emotion}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

def main():
    # 프로그램 시작 시 두 서버의 커넥션을 백그라운드 스레드로 각각 병렬 실행 (서로 간섭 방지)
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

    print(f"\n🚀 [듀얼 통신 AI 엔진] 가동 완료. 아두이노(TCP {ARDUINO_PORT}) & Qt(TCP {QT_PORT}) 연동 중...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        results = hands.process(rgb_frame) 
        
        now = time.time() 

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmarks = hand_landmarks.landmark
                
                fingers_open = []
                for tip, pip in
