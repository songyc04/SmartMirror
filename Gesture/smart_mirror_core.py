import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

# ── 내부 통신용 UDP 설정 (C++ Qt 프로그램의 9001번 포트로 전송) ──
UDP_IP = "127.0.0.1"
UDP_PORT = 9001
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

COOLDOWN_TIME = 1.5  # 제스처 오인식 방지 쿨타임 (초)

# ============================================================
#  DeepFace 표정 분석 기능 (별도 스레드로 실행하여 카메라 버벅임 방지)
# ============================================================
def analyze_emotion_async(frame):
    try:
        print("\n🎯 [DeepFace] 손동작 감지로 인한 표정 분석 시작...")
        # 젯슨 보드 최적화를 위해 가벼운 VGG-Face 모델 사용
        analysis = DeepFace.analyze(img_path=frame, actions=['emotion'], enforce_detection=False)
        
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        emotion = analysis["dominant_emotion"]
        print(f"✨ 분석된 감정 상태: [{emotion}]")

        # C++ Qt 프로그램이 읽을 수 있도록 UDP 전송
        # C++ MainWindow::gestureDetected()에서 "EMOTION_RESULT:감정"을 처리할 수 있게 보냅니다.
        msg = f"EMOTION_RESULT:{emotion}"
        sock.sendto(msg.encode('utf-8'), (UDP_IP, UDP_PORT))
        print(f"[UDP 전송 완료]: {msg}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

# ============================================================
#  메인 함수 & MediaPipe 손 제스처 엔진
# ============================================================
def main():
    # MediaPipe Hands 엔진 초기화 (오직 한 손 집중 마크)
    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

    # 카메라 열기 (젯슨 보드 최적화 해상도)
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    last_gesture = ""
    last_gesture_time = 0
    prev_y = 0

    print(f"\n🚀 [파이썬 AI 엔진] 가동 완료. 9001번 UDP 포트로 C++ Qt와 연동 중...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        # 거울 모드 적용 및 RGB 변환
        frame = cv2.flip(frame, 1)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # 오직 '손 관절'만 추적 (배경 및 평상시 얼굴 연산 낭비 차단)
        results = hands.process(rgb_frame)
        current_gesture = ""

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmarks = hand_landmarks.landmark
                
                # 손가락 펴짐 개수 확인 (주먹 vs 보자기)
                fingers_open = []
                for tip, pip in [(8, 6), (12, 10), (16, 14), (20, 18)]:
                    if landmarks[tip].y < landmarks[pip].y:
                        fingers_open.append(True)
                    else:
                        fingers_open.append(False)
                
                open_count = fingers_open.count(True)
                
                if open_count >= 3:
                    current_gesture = "START"   # 보자기 (HAND_OPEN)
                elif open_count == 0:
                    current_gesture = "STOP"    # 주먹 (HAND_FIST)

                # 손목 움직임 추정으로 상하 볼륨 스와이프 감지
                current_y = landmarks[0].y
                if prev_y != 0:
                    y_diff = current_y - prev_y
                    if y_diff < -0.15:   # 위로 쓸기
                        current_gesture = "SWIPE_UP"
                    elif y_diff > 0.15:  # 아래로 쓸기
                        current_gesture = "SWIPE_DOWN"
                prev_y = current_y
        else:
            prev_y = 0

        # ── 제스처 이벤트 트리거 로직 (쿨타임 반영 및 UDP 전송) ──
        if current_gesture and current_gesture != last_gesture:
            now = time.time()
            if now - last_gesture_time > COOLDOWN_TIME:
                print(f"✋ [손동작 인식]: {current_gesture}")
                
                # 1. 기본 제스처 명령을 즉시 C++ Qt로 전송
                msg = f"GESTURE:{current_gesture}"
                sock.sendto(msg.encode('utf-8'), (UDP_IP, UDP_PORT))
                
                # 2. 만약 보자기를 폈다면(`START`), 현재 프레임을 캡처하여 표정 분석 스레드 가동
                if current_gesture == "START":
                    # DeepFace 연산이 무거우므로 별도 스레드로 분리하여 카메라가 끊기지 않게 합니다.
                    threading.Thread(target=analyze_emotion_async, args=(frame.copy(),), daemon=True).start()

                last_gesture = current_gesture
                last_gesture_time = now

        if not current_gesture:
            last_gesture = ""

        # 초당 약 30 프레임 연산 주기 유지
        time.sleep(0.03)

    cap.release()

if __name__ == "__main__":
    main()
