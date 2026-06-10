import os
# QProcess 백그라운드 구동 시 발생하는 오디오 및 Protobuf 의존성 에러 차단 가드 치트키
os.environ["PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION"] = "python"
os.environ["PA_ALSA_PLUGHW"] = "1"

import sys
import cv2
import mediapipe as mp
import socket
import time
import threading
import speech_recognition as sr
import pyaudio

# ── 네트워크 설정 (오직 C++ Qt의 9001 포트로만 접속) ──────────────────────────
QT_IP = "127.0.0.1"           # 로컬 환경 Qt 서버 IP
QT_PORT = 9001                # Qt 서버가 대기 중인 TCP 포트

qt_socket = None              # Qt와 통신할 TCP 소켓
qt_connected = False

# ── 음성 인식 설정 ──
LANGUAGE        = "ko-KR"
WAKE_WORD       = "거울아"
COMMAND_TIMEOUT = 7  # Wake word 감지 후 명령 대기 시간 (초)

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

# ─ 제스처 쿨다운 및 타이머 설정 ──
SWIPE_COOLDOWN = 1.5       
VOLUME_COOLDOWN = 0.2      
HOLD_REQUIRED_TIME = 2.5   


# ── 🎤 [음성 인식 기능] 오디오 디바이스 감지 및 인식 스레드 ──
def get_microphone_index() -> int | None:
    pa = pyaudio.PyAudio()
    usb_index      = None
    loopback_index = None
    fallback_index = None

    for i in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(i)
        if info["maxInputChannels"] > 0:
            name = info["name"]
            if ("USB" in name or "Camera" in name) and usb_index is None:
                usb_index = i
            if "Loopback" in name and loopback_index is None:
                loopback_index = i
            if fallback_index is None:
                fallback_index = i
    pa.terminate()

    if usb_index is not None: return usb_index
    if loopback_index is not None: return loopback_index
    return fallback_index

def listen_once(recognizer, source, timeout=5, phrase_limit=5) -> str | None:
    try:
        audio = recognizer.listen(source, timeout=timeout, phrase_time_limit=phrase_limit)
        text = recognizer.recognize_google(audio, language=LANGUAGE)
        return text
    except (sr.WaitTimeoutError, sr.UnknownValueError):
        return None
    except sr.RequestError as e:
        print(f"[서버 오류] {e}", flush=True)
        return None

def voice_controller_thread():
    """ 실시간으로 마이크를 감시하며 '헤이 미러'와 음악 검색 명령을 처리하는 스레드 """
    mic_index = get_microphone_index()
    if mic_index is None:
        print("[오류] 사용 가능한 마이크가 없습니다. 음성인식을 종료합니다.", flush=True)
        return

    r = sr.Recognizer()
    mic = sr.Microphone(device_index=mic_index)

    print("[초기화] 주변 소음 설정 중...", flush=True)
    with mic as source:
        r.adjust_for_ambient_noise(source, duration=2)

    print(f"[대기 중] '{WAKE_WORD}' 라고 말하면 음성 인식이 활성화됩니다.", flush=True)

    while True:
        # Qt 서버 연결이 확보되었을 때만 음성 인식 가동
        if not qt_connected:
            time.sleep(1)
            continue
            
        try:
            with mic as source:
                # 1단계: Wake word ("헤이 미러") 대기
                text = listen_once(r, source, timeout=5, phrase_limit=4)
                if text is None or WAKE_WORD not in text:
                    continue

                print(f"\n🗣️ [호출 감지] '{text}' 수신 완료.", flush=True)
                print(f"[활성화] 명령을 말씀하세요. ({COMMAND_TIMEOUT}초 내)", flush=True)

                # 2단계: "~ 틀어줘" 음악 재생 검색어 대기
                command_text = listen_once(r, source, timeout=COMMAND_TIMEOUT, phrase_limit=5)

                if command_text is None:
                    print("[비활성화] 명령을 듣지 못했습니다.", flush=True)
                    continue

                print(f"[명령 수신] {command_text}", flush=True)

                # "~ 틀어줘", "~ 재생해줘" 문형 판별 및 정제
                if any(keyword in command_text for keyword in ["틀어줘", "틀어 줘", "재생해줘", "들려줘", "찾아줘"]):
                    search_query = command_text
                    for keyword in ["틀어줘", "틀어 줘", "재생해줘", "들려줘", "찾아줘"]:
                        search_query = search_query.replace(keyword, "")
                    search_query = search_query.strip()

                    if search_query:
                        print(f"🎵 [유튜브 플레이리스트 요청] 검색어: {search_query}", flush=True)
                        
                        # 팩션 1: Qt의 keyword 변수에 값 선치행 저장
                        send_to_qt(f"KEYWORD:{search_query}")
                        
                        # TCP 버퍼에서 패킷이 뭉쳐 파싱 에러가 나는 것을 방지하는 정밀 딜레이
                        time.sleep(0.15)
                        
                        # 팩션 2: Qt의 gesture 변수에 "START" 상태를 트리거하여 음악을 실행
                        send_to_qt("START")
                    else:
                        print("[오류] 검색어가 비어있습니다.", flush=True)
                else:
                    # 사전에 없더라도 키워드 추출 시도 방어 코드
                    send_to_qt(f"KEYWORD:{command_text.strip()}")
                    time.sleep(0.15)
                    send_to_qt("START")

        except Exception as e:
            print(f"[음성 엔진 오류] {e}", flush=True)
            time.sleep(1)


# ── 👤 [얼굴 위치 인식] 사용자 좌/우 위치 판단 스레드 ──
face_position_running = True
last_user_position = None

def face_position_thread():
    global face_position_running, last_user_position

    mp_face = mp.solutions.face_detection
    face_detection = mp_face.FaceDetection(model_selection=0, min_detection_confidence=0.5)

    print("[얼굴 위치] 사용자 위치 감지 스레드 시작 (3초 주기)", flush=True)

    while face_position_running:
        time.sleep(3)

        if not qt_connected:
            continue

        with gesture_lock:
            if latest_rgb_frame is None:
                continue
            frame_copy = latest_rgb_frame.copy()

        results = face_detection.process(frame_copy)

        if results.detections:
            for detection in results.detections:
                bbox = detection.location_data.relative_bounding_box
                face_center_x = bbox.xmin + bbox.width / 2.0

                if face_center_x < 0.5:
                    current_position = "LEFT"
                else:
                    current_position = "RIGHT"

                if current_position != last_user_position:
                    last_user_position = current_position
                    msg = f"USER:{current_position}"
                    print(f"👤 [위치 감지] {msg} -> Qt 송신", flush=True)
                    send_to_qt(msg)
                break

    face_detection.close()


# ── ✋ MediaPipe 손 인식을 처리할 비동기 백그라운드 함수 ──
gesture_lock = threading.Lock()
latest_rgb_frame = None
gesture_running = True

def gesture_processing_thread(hands):
    global latest_rgb_frame, gesture_running, qt_connected
    
    last_swipe_time = 0
    last_volume_time = 0
    prev_x, prev_y = 0, 0       
    swipe_x_accum = 0.0
    swipe_prev_ref_x = 0.0
    
    start_hold_start_time = 0   
    stop_hold_start_time = 0    
    end_hold_start_time = 0     
    
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
                
                base_fingers_open = []
                for tip, pip in [(8, 6), (12, 10), (16, 14), (20, 18)]: 
                    if landmarks[tip].y < landmarks[pip].y:
                        base_fingers_open.append(True)  
                    else:
                        base_fingers_open.append(False) 
                
                four_fingers_count = base_fingers_open.count(True)

                if four_fingers_count == 0:
                    open_count = 0
                elif four_fingers_count == 1:
                    open_count = 1
                else:
                    thumb_open = landmarks[4].y < landmarks[2].y or abs(landmarks[4].x - landmarks[2].x) > 0.05
                    open_count = four_fingers_count + (1 if thumb_open else 0)

                is_start_hand = (open_count == 5)   
                is_stop_hand = (open_count == 1)    
                is_end_hand = (open_count == 0)     

                # 1) START 판정 (손가락 5개) -> 기존 음악 재생/재개
                if is_start_hand:
                    stop_hold_start_time = 0  
                    stop_triggered = False
                    end_hold_start_time = 0
                    end_triggered = False
                    
                    if start_hold_start_time == 0:
                        start_hold_start_time = now  
                    elif now - start_hold_start_time >= HOLD_REQUIRED_TIME and not start_triggered:
                        print("✋ [제스처 감지] GESTURE:START -> Qt 송신", flush=True)
                        send_to_qt("START")
                        start_triggered = True 
                        
                # 2) STOP 판정 (손가락 1개) -> 음악 일시정지
                elif is_stop_hand:
                    start_hold_start_time = 0 
                    start_triggered = False
                    end_hold_start_time = 0
                    end_triggered = False
                    
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now  
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("☝️ [제스처 감지] GESTURE:STOP -> Qt 송신", flush=True)
                        send_to_qt("STOP")
                        stop_triggered = True 
                        
                # 3) END 판정 (주먹) -> 프로세스 완전 종료
                elif is_end_hand:
                    start_hold_start_time = 0
                    start_triggered = False
                    stop_hold_start_time = 0
                    stop_triggered = False
                    
                    if end_hold_start_time == 0:
                        end_hold_start_time = now
                    elif now - end_hold_start_time >= HOLD_REQUIRED_TIME and not end_triggered:
                        print("✊ [제스처 감지] GESTURE:END -> Qt 송신", flush=True)
                        send_to_qt("END")
                        end_triggered = True
                        
                else:
                    start_hold_start_time = 0
                    stop_hold_start_time = 0
                    end_hold_start_time = 0
                    start_triggered = False
                    stop_triggered = False
                    end_triggered = False

                # ── 볼륨 및 패널 스와이프 제어 로직 ──
                current_x = landmarks[0].x
                current_y = landmarks[0].y

                if prev_x != 0 and prev_y != 0:
                    y_diff = current_y - prev_y 
                    x_diff = current_x - prev_x 

                    if now - last_volume_time > VOLUME_COOLDOWN:
                        if y_diff < -0.08:  
                            print("▲ [볼륨] VOLUME_UP", flush=True)
                            send_to_qt("VOLUME_UP")
                            last_volume_time = now
                        elif y_diff > 0.08: 
                            print("▼ [볼륨] VOLUME_DOWN", flush=True)
                            send_to_qt("VOLUME_DOWN")
                            last_volume_time = now

                    if now - last_swipe_time > SWIPE_COOLDOWN:
                        ref_x = (landmarks[0].x + landmarks[9].x) / 2.0

                        if swipe_prev_ref_x != 0:
                            swipe_x_accum += (ref_x - swipe_prev_ref_x)

                        swipe_prev_ref_x = ref_x

                        if swipe_x_accum < -0.18:
                            print("◀ [스와이프] LEFT 패널", flush=True)
                            send_to_qt("LEFT")
                            last_swipe_time = now
                            swipe_x_accum = 0.0
                            swipe_prev_ref_x = 0.0
                        elif swipe_x_accum > 0.18:
                            print("▶ [스와이프] RIGHT 패널", flush=True)
                            send_to_qt("RIGHT")
                            last_swipe_time = now
                            swipe_x_accum = 0.0
                            swipe_prev_ref_x = 0.0
                    else:
                        swipe_prev_ref_x = 0.0
                        swipe_x_accum = 0.0

                prev_x = current_x
                prev_y = current_y
        else:
            prev_x, prev_y = 0, 0
            swipe_prev_ref_x = 0.0
            swipe_x_accum = 0.0
            start_hold_start_time = 0
            stop_hold_start_time = 0
            end_hold_start_time = 0
            start_triggered = False
            stop_triggered = False
            end_triggered = False
            
        time.sleep(0.02)


def main():
    global latest_rgb_frame, gesture_running
    
    # 1. Qt TCP 클라이언트 스레드 기동
    threading.Thread(target=connect_to_qt, daemon=True).start()

    # 2. 🎤 음성 인식 제어 스레드 백그라운드 기동 (얼굴인식 완전 대체)
    threading.Thread(target=voice_controller_thread, daemon=True).start()

    # 3. MediaPipe 손 제어 엔진 로드
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
        print("\n❌ PYTHON_ERROR: 카메라를 감지할 수 없습니다.", flush=True)
        sys.exit(1)
        
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    # 4. 손 제어 연산 스레드 가동
    t = threading.Thread(target=gesture_processing_thread, args=(hands,), daemon=True)
    t.start()

    # 5. 👤 얼굴 위치 감지 스레드 가동
    threading.Thread(target=face_position_thread, daemon=True).start()

    print(f"\n🚀 [제스처/음성/위치 통합 AI 엔진] 가동 완료. 모니터링 시작...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.005)
            continue

        frame = cv2.flip(frame, 1) 
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) 
        
        with gesture_lock:
            latest_rgb_frame = rgb_frame

        if cv2.waitKey(10) & 0xFF == ord('q'):
            break

    gesture_running = False
    face_position_running = False
    if qt_socket:
        qt_socket.close()
    cap.release()

if __name__ == "__main__":
    main()
