import cv2
import mediapipe as mp
import socket
import time
import threading
from deepface import DeepFace

# ── 네트워크 통신 및 글로벌 소켓 설정 ──
TCP_IP = "127.0.0.1"      # 루프백(Localhost) 주소: 현재 PC 내부 통신용
TCP_PORT = 9001           # Qt 또는 ESP32 서버와 매칭할 포트 번호
client_socket = None      # TCP 클라이언트 소켓 객체 저장용 전역 변수

# ── [함수] TCP 서버 자동 재연결 로직 ──
def connect_to_server():
    global client_socket
    while True:
        try:
            # 소켓 객체가 비어있을 때만 새로 생성 및 연결 시도
            if client_socket is None:
                print(f"🔄 C++ Qt TCP 서버 연결 시도 중 ({TCP_IP}:{TCP_PORT})...")
                client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client_socket.connect((TCP_IP, TCP_PORT))
                print("✅ C++ Qt TCP 서버와 연결 성공!")
                break # 연결에 성공하면 무한 루프 탈출
        except Exception as e:
            # 서버가 켜지지 않은 경우 2초 간격으로 계속 재접속을 시도 (크래시 방지)
            print(f"❌ 연결 실패 ({e}). 2초 후 재시도합니다.")
            client_socket = None
            time.sleep(2)

# ── [함수] 데이터 안전 송신 로직 (개행문자 포함) ──
def send_tcp_message(message):
    global client_socket
    if client_socket is None:
        return # 연결이 없는 상태라면 전송 취소
    try:
        # 상대방 서버(Qt 등)가 한 줄 단위(readLine)로 쉽게 읽을 수 있게 끝에 '\n' 부착
        full_msg = message + "\n"
        client_socket.sendall(full_msg.encode('utf-8'))
    except Exception as e:
        # 송신 중 에러가 발생하면 연결이 끊긴 것으로 간주하고 소켓 초기화 후 재연결 백그라운드 구동
        print(f"❌ TCP 전송 중 오류 발생 ({e}). 서버 연결이 끊어진 것 같습니다.")
        client_socket = None
        threading.Thread(target=connect_to_server, daemon=True).start()

# ── 제스처 인식 감도 및 시간 제한(타이밍) 상수 정의 ──
SWIPE_COOLDOWN = 1.5       # 좌우 스와이프(페이지 전환) 후 인정 안 하는 재사용 대기시간 (초)
VOLUME_COOLDOWN = 0.2      # 상하 볼륨 조절의 부드러운 연속 입력을 위한 최소 시간 간격 (초)
HOLD_REQUIRED_TIME = 1.5   # 재생(START) / 정지(STOP) 인식을 위해 손 모양을 유지해야 하는 목표 시간 (초)

# ── [함수] DeepFace 감정 분석 로직 (카메라 프레임 드랍 방지를 위해 멀티스레드 가동) ──
def analyze_emotion_async(frame):
    try:
        print("\n🎯 [DeepFace] 노래 추천을 위한 표정 분석 시작...")
        # 연산 속도 최적화(FPS 확보)를 위해 프레임 해상도를 가볍게 다운스케일링
        small_frame = cv2.resize(frame, (320, 240))
        # 속도가 빠른 기본 감정 모델 분석 가동 (얼굴 미검출 시 예외 처리 방지 설정)
        analysis = DeepFace.analyze(img_path=small_frame, actions=['emotion'], enforce_detection=False)
        
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        emotion = analysis["dominant_emotion"] # 가장 지배적인 감정 문자열 추출 (예: happy, sad 등)
        print(f"✨ 분석된 감정 상태: [{emotion}]")

        # 분석 완료된 감정 카테고리를 TCP 채널을 통해 서버로 즉시 송신
        send_tcp_message(f"EMOTION_RESULT:{emotion}")
        print(f"[TCP 전송 완료] EMOTION_RESULT:{emotion}")

    except Exception as e:
        print(f"❌ DeepFace 분석 중 오류 발생: {e}")

# ── 메인 실행 루프 ──
def main():
    connect_to_server() # 프로그램 실행 시 최초 1회 TCP 서버 접속 시도

    # MediaPipe Hands 모델 초기화 (한 손 집중 제어 모드, 신뢰도 70% 설정)
    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

    # 카메라 입력 가동 (Linux V4L2 최적화 640x480 해상도)
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    # 제스처 타임스탬프 및 위치 기억 변수
    last_swipe_time = 0
    last_volume_time = 0
    prev_x, prev_y = 0, 0       # 궤적을 추적하기 위한 이전 루프의 손목(Landmark 0) 좌표
    swipe_locked = False        # 쓸기 동작 후 손이 중앙으로 복귀할 때 생기는 오인식 방지용 락 플래그

    # 1.5초 유지형 제스처(재생/정지) 카운팅을 위한 시간 관리 변수
    start_hold_start_time = 0   # 보자기(재생)를 펴기 시작한 시점의 타임스탬프
    stop_hold_start_time = 0    # 주먹(정지)을 쥐기 시작한 시점의 타임스탬프
    start_triggered = False     # 재생 신호 중복 전송 방지용 플래그
    stop_triggered = False      # 정지 신호 중복 전송 방지용 플래그

    print(f"\n🚀 [업그레이드 AI 엔진] 가동 완료. TCP {TCP_PORT}번 포트로 통신 중...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1) # 사용자 편의성을 위한 거울 모드(좌우 반전) 처리
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) # MediaPipe 인식용 RGB 채널 변환
        results = hands.process(rgb_frame) # 손 관절 실시간 연산 프로세스 실행
        
        now = time.time() # 실시간 제스처 유효 타이밍 측정을 위한 현재 시간 갱신

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                landmarks = hand_landmarks.landmark
                
                # [1단계] 손가락 펴짐 개수 연산 알고리즘 (끝마디 Y좌표 < 중간마디 Y좌표 판별)
                fingers_open = []
                for tip, pip in [(8, 6), (12, 10), (16, 14), (20, 18)]: # 검지, 중지, 약지, 새끼
                    if landmarks[tip].y < landmarks[pip].y:
                        fingers_open.append(True)  # 펴짐
                    else:
                        fingers_open.append(False) # 접힘
                open_count = fingers_open.count(True)

                is_open_hand = (open_count >= 3) # 손가락이 3개 이상 펴지면 보자기(재생 상태)로 정의
                is_fist_hand = (open_count == 0) # 모든 손가락이 접히면 주먹(정지 상태)으로 정의

                # [2단계] 1.5초 유지형 정면 제스처 트리거 제어 로직
                if is_open_hand:
                    stop_hold_start_time = 0  # 반대 제스처인 주먹 타이머 초기화
                    stop_triggered = False
                    
                    if start_hold_start_time == 0:
                        start_hold_start_time = now # 처음 펼치기 시작한 순간 시간 기록
                    elif now - start_hold_start_time >= HOLD_REQUIRED_TIME and not start_triggered:
                        # 손을 편 채로 1.5초 버티기 성공 시 발동
                        print("✋ [유지 감지] 1.5초간 보자기 유지 -> 재생(START) 및 감정 분석 실행")
                        send_tcp_message("GESTURE:START")
                        # 무거운 DeepFace 연산은 데몬 스레드로 돌려 메인 카메라 스트리밍이 끊기지 않게 분리
                        threading.Thread(target=analyze_emotion_async, args=(frame.copy(),), daemon=True).start()
                        start_triggered = True # 연속 발동 방지 잠금
                        
                elif is_fist_hand:
                    start_hold_start_time = 0 # 반대 제스처인 보자기 타이머 초기화
                    start_triggered = False
                    
                    if stop_hold_start_time == 0:
                        stop_hold_start_time = now # 처음 쥐기 시작한 순간 시간 기록
                    elif now - stop_hold_start_time >= HOLD_REQUIRED_TIME and not stop_triggered:
                        # 주먹을 쥔 채로 1.5초 버티기 성공 시 발동
                        print("✊ [유지 감지] 1.5초간 주먹 유지 -> 멈춤(STOP)")
                        send_tcp_message("GESTURE:STOP")
                        stop_triggered = True # 연속 발동 방지 잠금
                else:
                    # 손 모양이 애매하게 흐트러지면 유지시간 카운팅 변수 및 잠금 올 리셋
                    start_hold_start_time = 0
                    stop_hold_start_time = 0
                    start_triggered = False
                    stop_triggered = False

                # [3단계] 손목(0번 랜드마크)을 기준으로 정밀 변위 계산 (동작 추적)
                current_x = landmarks[0].x
                current_y = landmarks[0].y

                if prev_x != 0 and prev_y != 0:
                    y_diff = current_y - prev_y # 상하 이동 거리 변위
                    x_diff = current_x - prev_x # 좌우 이동 거리 변위

                    # [A] 상하 볼륨 제어 파트 (0.2초 쿨타임 주기마다 미세 동작 수용)
                    if now - last_volume_time > VOLUME_COOLDOWN:
                        if y_diff < -0.08:  # 위로 휙 올릴 때 (픽셀 기준 상단이 값 하강)
                            print("▲ [볼륨 업] VOLUME_UP")
                            send_tcp_message("GESTURE:VOLUME_UP")
                            last_volume_time = now
                        elif y_diff > 0.08: # 아래로 휙 내릴 때
                            print("▼ [볼륨 다운] VOLUME_DOWN")
                            send_tcp_message("GESTURE:VOLUME_DOWN")
                            last_volume_time = now

                    # [B] 좌우 스와이프 파트 (1.5초 대형 쿨타임 및 왕복 복귀 오인식 차단 기능 장착)
                    if now - last_swipe_time > SWIPE_COOLDOWN:
                        if not swipe_locked:
                            # 락이 풀려있을 때만 진정한 최초 이동 명령으로 수용
                            if x_diff < -0.14:   # 왼쪽으로 강하게 쓸기
                                print("◀ [스와이프] 왼쪽으로 쓸기 -> 다음 페이지(NEXT)")
                                send_tcp_message("GESTURE:NEXT")
                                last_swipe_time = now
                                swipe_locked = True # 제자리로 손이 리턴할 때 반대 명령이 튀는 것을 차단하기 위해 락 체결
                            elif x_diff > 0.14:  # 오른쪽으로 강하게 쓸기
                                print("▶ [스와이프] 오른쪽으로 쓸기 -> 이전 페이지(PREV)")
                                send_tcp_message("GESTURE:PREV")
                                last_swipe_time = now
                                swipe_locked = True # 제자리로 손이 리턴할 때 반대 명령이 튀는 것을 차단하기 위해 락 체결
                        else:
                            # 락이 묶인 상태에서, 쓸었던 손이 다시 중앙 정면 제자리로 수렴하는 움직임 포착 시 유연하게 락 해제
                            if (x_diff > 0.05 or x_diff < -0.05):
                                swipe_locked = False

                # 다음 루프 비교 연산을 위해 현재 좌표를 백업
                prev_x = current_x
                prev_y = current_y
        else:
            # 손이 카메라 앵글 밖으로 벗어나면 누적 위치 데이터 및 락 상태 전면 초기화
            prev_x, prev_y = 0, 0
            swipe_locked = False
            start_hold_start_time = 0
            stop_hold_start_time = 0
            start_triggered = False
            stop_triggered = False

        # 약 30FPS의 안정적인 영상 연산 주기를 확보하여 오버헤드 차단
        time.sleep(0.03)

    # 루프 탈출 시 소켓 및 카메라 자원 안전 반환
    if client_socket:
        client_socket.close()
    cap.release()

if __name__ == "__main__":
    main()
