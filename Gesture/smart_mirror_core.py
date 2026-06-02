import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

TCP_IP = "127.0.0.1"
TCP_PORT = 9001  
client_socket = None

def connect_to_server():
    global client_socket
    while True:
        try:
            if client_socket is None:
                print(f"🔄 C++ Qt TCP 서버 연결 시도 중 ({TCP_IP}:{TCP_PORT})...")
                client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client_socket.connect((TCP_IP, TCP_PORT))
                print("✅ C++ Qt TCP 서버와 연결 성공!")
                break
        except Exception as e:
            print(f"❌ 연결 실패 ({e}). 2초 후 재시도합니다.")
            client_socket = None
            time.sleep(2)

def send_tcp_message(message):
    global client_socket
    if client_socket is None:
        return
    try:
        full_msg = message + "\n"
        client_socket.sendall(full_msg.encode('utf-8'))
    except Exception as e:
        print(f"❌ TCP 전송 중 오류 발생 ({e}). 서버 연결이 끊어진 것 같습니다.")
        client_socket = None
        threading.Thread(target=connect_to_server, daemon=True).start()

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

        send_tcp_message(f"EMOTION_RESULT:{emotion}")
        print(f"[TCP 전송 완료] EMOTION_RESULT:{emotion}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

def main():
    connect_to_server()

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

    print(f"\n🚀 [업그레이드 AI 엔진] 가동 완료. TCP {TCP_PORT}번 포트로 통신 중...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(rgb_frame)
