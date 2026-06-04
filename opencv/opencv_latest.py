import sys
import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

# ── 네트워크 설정 (오직 C++ Qt의 9001 포트로만 접속) ──────────────────────────
QT_IP = "127.0.0.1"           # 로컬 환경 Qt 서버 IP
QT_PORT = 9001                # Qt 서버가 대기 중인 TCP 포트

qt_socket = None              # Qt와 통신할 TCP 소켓
qt_connected = False

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

# ── [통신] C++ Qt TCP 서버 자동 연결 및 유지 로직 ──
def connect_to_qt():
    global qt_socket, qt_connected
    while True:
        try:
            if qt_socket is None:
                print(f"🔄 Qt 서버 연결 시도 중 ({QT_IP}:{QT_PORT})...", flush=True)
                qt_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                qt_socket.settimeout(3.0)
                qt_socket.connect((QT_IP, QT_PORT))
                print("✅ C++ Qt 서버(9001)와 성공적으로 연결되었습니다!", flush=True)
                qt_connected = True
                break
        except Exception as e:
            print(f"❌ Qt 서버 연결 실패 ({e}). 2초 후 재시도합니다.", flush=True)
            qt_socket = None
            qt_connected = False
            time.sleep(2)

# ── [통신] Qt 서버로 메시지 단방향 전송 ──
def send_to_qt(message):
    global qt_socket, qt_connected
    full_msg = message + "\n"
    encoded_msg = full_msg.encode('utf-8')
    
    if qt_socket is not None:
        try:
            qt_socket.sendall(encoded_msg)
        except Exception as e:
            print(f"❌ Qt 서버 전송 실패 ({e}). 재연결을 진행합니다.", flush=True)
            qt_socket = None
            qt_connected = False
            threading.Thread(target=connect_to_qt, daemon=True).start()

# ── 제스처 쿨다운 및 타이머 설정 ──
SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 1.5   

# ── [AI 기능] 감정 스캔 및 결과 전송 함수 ──
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

        send_to_qt(f"EMOTION_RESULT:{upper_emotion}")
        send_to_qt(f"PLAY_SONG:{recommended_song}")

    except Exception as e:
        print(f"PYTHON_ERROR (DeepFace): {str(e)}", flush=True)


# ── MediaPipe 손 인식을 처리할 비동기 백그라운드 함수 ──
gesture_lock = threading.Lock()
latest_rgb_frame = None
gesture_running = True

def gesture_processing_thread(hands):
    global latest_rgb_frame, gesture_running, qt_connected
    
    # 제스처 상태 변수들
    last_swipe_time = 0
    last_volume_time = 0
    prev_x, prev_y = 0, 0       
    swipe_locked = False        
    
    # 각 상태별 유지 타이머 누적 변수
    start_hold_start_time = 0   
    stop_hold_start_time = 0    
    end_hold_start_time = 0     
    
    # 트리거 작동 완료 플래그
    start_triggered = False     
    stop_triggered = False      
    end_triggered = False         

    while gesture_running:
        with gesture_lock:
            if latest_rgb_frame is None:
                local_frame = None
            else:
                local_frame = latest_rgb_frame.copy()
        
        if local_frame is None:
            time.sleep(0.01)
            continue
            
        now = time.time()
        results = hands.process(local_frame)

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmarks = hand_landmarks.landmark
                
                # 1. 네 손가락(검지, 중지, 약지, 새끼)의 펴짐 상태 확인
                base_fingers_open = []
                for tip, pip in [(8, 6), (12, 10), (16, 14), (20, 18)]: 
                    if landmarks[tip].y < landmarks[pip].y:
                        base_fingers_open.append(True)  
                    else:
                        base_fingers_open.append(False) 
                
                # 4개의 주요 손가락 중 펴진 개수 계산
                four_fingers_count = base_fingers_open.count(True)

                # 2. 💡 [이중 방어 필터 적용] 엄지 오인식 완벽 차단
                # 조건 A: 네 손가락이 전부 접혔다 -> 엄지 무시하고 무조건 0개 (주먹: END)
                if four_fingers_count == 0:
                    open_count = 0
                
                # 조건 B: 검지/중지/약지/새끼 중 정확히 '하나'만 펴졌다 -> 엄지 상태가 어떻든 무조건 1개 (STOP)
                elif four_fingers_count == 1:
                    open_count = 1
                    
                # 조건 C: 손을 애매하게 폈거나 전부 다 폈을 때만 엄지 상태를 정밀 합산
                else:
                    thumb_open = landmarks[4].y < landmarks[2].y or abs(landmarks[4].x - landmarks[2].x) > 0.05
                    open_count = four_fingers_count + (1 if thumb_open else 0)

                # 3. 보정된 개수로 제스처 매핑
                is_start_hand = (open_count == 5)   # START: 손가락 5개 전부 폄
                is_stop_hand = (open_count == 1)    # STOP: 손가락 1개만 폄 (검지 하나만 핀 상태)
                is_end_hand = (open_count == 0)     # END: 손가락 0개 (주먹 쥔 상태)

                # 1) START 판정 (손가락 5개)
                if is_start_hand:
                    stop_hold_start_time = 0  
                    stop_triggered = False
                    end_hold_start_time = 0
                    end_triggered = False
                    
                    if start_hold_start_time == 0:
                        start_hold_start_time = now 
                    elif now - start_hold_start_time >= HOLD_REQUIRED_TIME and not start_triggered:
                        print("✋ [유지 감지] 손가락 5개 -> GESTURE:START", flush=True)
                        send_to_qt("START")
                        start_triggered = True 
                        
                # 2) STOP 판정 (손가락 1개)
                elif is_stop_hand:
                    start_hold_start_time = 0 
                    start_triggered = False
                    end_hold_start_time = 0
                    end_triggered = False
                    
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now 
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("☝️ [유지 감지] 손가락 1개 -> GESTURE:STOP", flush=True)
                        send_to_qt("STOP")
                        stop_triggered = True 
                        
                # 3) END 판정 (손가락 0개 / 주먹)
                elif is_end_hand:
                    start_hold_start_time = 0
                    start_triggered = False
                    stop_hold_start_time = 0
                    stop_triggered = False
                    
                    if end_hold_start_time == 0:
                        end_hold_start_time = now
                    elif now - end_hold_start_time >= HOLD_REQUIRED_TIME and not end_triggered:
                        print("✊ [유지 감지] 주먹(0개) -> GESTURE:END", flush=True)
                        send_to_qt("END")
                        end_triggered = True
                        
                else:
                    start_hold_start_time = 0
                    stop_hold_start_time = 0
                    end_hold_start_time = 0
                    start_triggered = False
                    stop_triggered = False
                    end_triggered = False

                # ── 기존 볼륨 및 스와이프 로직 유지 ──
                current_x = landmarks[0].x
                current_y = landmarks[0].y

                if prev_x != 0 and prev_y != 0:
                    y_diff = current_y - prev_y 
                    x_diff = current_x - prev_x 

                    if now - last_volume_time > VOLUME_COOLDOWN:
                        if y_diff < -0.08:  
                            print("▲ [볼륨 업] VOLUME_UP", flush=True)
                            send_to_qt("VOLUME_UP")
                            last_volume_time = now
                        elif y_diff > 0.08: 
                            print("▼ [볼륨 다운] VOLUME_DOWN", flush=True)
                            send_to_qt("VOLUME_DOWN")
                            last_volume_time = now

                    if now - last_swipe_time > SWIPE_COOLDOWN:
                        if not swipe_locked:
                            if x_diff < -0.14:   
                                print("◀ [스와이프] 다음 페이지(NEXT)", flush=True)
                                send_to_qt("NEXT")
                                last_swipe_time = now
                                swipe_locked = True 
                            elif x_diff > 0.14:  
                                print("▶ [스와이프] 이전 페이지(PREV)", flush=True)
                                send_to_qt("PREV")
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
            end_hold_start_time = 0
            start_triggered = False
            stop_triggered = False
            end_triggered = False
            
        time.sleep(0.02)


def main():
    global latest_rgb_frame, gesture_running
    
    threading.Thread(target=connect_to_qt, daemon=True).start()

    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(
        max_num_hands=1, 
        min_detection_confidence=0.5, 
        min_tracking_confidence=0.5
    )

    cap = None
    for cam_idx in range(10):
        print(f"📷 카메라 인덱스 [{cam_idx}]번 오픈 시도 중...", flush=True)
        cap = cv2.VideoCapture(cam_idx, cv2.CAP_V4L2)
        if cap.isOpened():
            print(f"✅ 카메라 [{cam_idx}]번 장치를 성공적으로 열었습니다!", flush=True)
            break
        cap.release()

    if not cap or not cap.isOpened():
        print("\n❌ PYTHON_ERROR: 모든 인덱스에서 카메라를 열 수 없습니다.", flush=True)
        sys.exit(1)
        
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    initial_scan_done = False
    scan_start_time = 0

    t = threading.Thread(target=gesture_processing_thread, args=(hands,), daemon=True)
    t.start()

    print(f"\n🚀 [제스처/감정 AI 엔진] 가동 완료. 스트리밍 시작...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.005)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        
        with gesture_lock:
            latest_rgb_frame = rgb_frame

        now = time.time() 

        if qt_connected and not initial_scan_done:
            if scan_start_time == 0:
                scan_start_time = now
                print("\n⏳ [이벤트] Qt 연동 완료! 5초 후 표정을 스캔합니다...", flush=True)
            elif now - scan_start_time >= 5.0:
                print("📸 찰칵! 표정 데이터를 캡처했습니다.", flush=True)
                threading.Thread(target=analyze_emotion_and_play, args=(frame.copy(),), daemon=True).start()
                initial_scan_done = True 

        cv2.imshow("SmartMirror Camera", frame)

        if cv2.waitKey(10) & 0xFF == ord('q'):
            print("\n⏹️ 사용자에 의해 카메라 모니터링이 종료되었습니다.", flush=True)
            break

    gesture_running = False
    if qt_socket:
        qt_socket.close()
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
