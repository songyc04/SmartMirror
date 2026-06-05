import sys
import cv2
import mediapipe as mp
import socket
import time
import threading
from collections import deque
from deepface import DeepFace

# ── 네트워크 설정 ─────────────────────────────────────────────────────────────
QT_IP   = "127.0.0.1"
QT_PORT = 9001

qt_socket    = None
qt_connected = False

# ── 재생 상태 플래그 (Qt로부터 수신) ─────────────────────────────────────────
# Qt 서버가 "PLAYING\n" 을 보내오면 True, "STOPPED\n" / "PAUSED\n" 이면 False
is_playing      = False
is_playing_lock = threading.Lock()

# ── 감정별 맞춤 노래 맵핑 ─────────────────────────────────────────────────────
MUSIC_MAP = {
    "HAPPY":    "신나는 댄스곡",
    "SAD":      "잔잔한 위로의 발라드",
    "ANGRY":    "스트레스가 풀리는 록/힙합",
    "SURPRISE": "통통 튀는 팝송",
    "NEUTRAL":  "마음이 편안해지는 어쿠스틱/재즈",
    "FEAR":     "차분한 클래식",
    "DISGUST":  "기분 전환용 트로피컬 하우스",
}

# ═══════════════════════════════════════════════════════════════════════════════
# [통신] Qt TCP 연결 / 전송 / 수신
# ═══════════════════════════════════════════════════════════════════════════════

def connect_to_qt():
    """Qt 서버에 연결될 때까지 재시도. 연결 성공 시 수신 스레드도 시작."""
    global qt_socket, qt_connected
    while True:
        try:
            if qt_socket is None:
                print(f"🔄 Qt 서버 연결 시도 중 ({QT_IP}:{QT_PORT})...", flush=True)
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(3.0)
                s.connect((QT_IP, QT_PORT))
                s.settimeout(None)          # 수신 스레드용 블로킹 모드 전환
                qt_socket    = s
                qt_connected = True
                print("✅ C++ Qt 서버(9001)와 성공적으로 연결되었습니다!", flush=True)
                # Qt → Python 상태 수신 스레드 시작
                threading.Thread(target=qt_receive_thread, daemon=True).start()
                break
        except Exception as e:
            print(f"❌ Qt 서버 연결 실패 ({e}). 2초 후 재시도합니다.", flush=True)
            qt_socket    = None
            qt_connected = False
            time.sleep(2)


def qt_receive_thread():
    """
    Qt 서버가 보내는 상태 메시지를 수신해 is_playing 플래그를 갱신한다.

    Qt 측에서 아래 형식으로 송신하면 됩니다:
        "PLAYING\n"  → 음악 재생 중  (START 중복 차단 활성화)
        "STOPPED\n"  → 정지 상태    (START 허용)
        "PAUSED\n"   → 일시정지     (START 허용)
    """
    global qt_socket, qt_connected, is_playing
    buf = ""
    while True:
        try:
            chunk = qt_socket.recv(1024)
            if not chunk:
                raise ConnectionResetError("서버 연결 종료")
            buf += chunk.decode("utf-8", errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                print(f"[Qt→Py] 수신: {line}", flush=True)
                with is_playing_lock:
                    if line == "PLAYING":
                        is_playing = True
                        print("🎵 재생 상태 확인 → START 중복 차단 활성화", flush=True)
                    elif line in ("STOPPED", "PAUSED"):
                        is_playing = False
                        print("⏹️  정지/일시정지 확인 → START 허용", flush=True)
        except Exception as e:
            print(f"❌ Qt 수신 스레드 오류 ({e}). 재연결을 시도합니다.", flush=True)
            with is_playing_lock:
                is_playing = False
            qt_socket    = None
            qt_connected = False
            threading.Thread(target=connect_to_qt, daemon=True).start()
            break


def send_to_qt(message):
    global qt_socket, qt_connected
    encoded = (message + "\n").encode("utf-8")
    if qt_socket is not None:
        try:
            qt_socket.sendall(encoded)
        except Exception as e:
            print(f"❌ Qt 서버 전송 실패 ({e}). 재연결을 진행합니다.", flush=True)
            qt_socket    = None
            qt_connected = False
            threading.Thread(target=connect_to_qt, daemon=True).start()


# ═══════════════════════════════════════════════════════════════════════════════
# [AI] 감정 분석
# ═══════════════════════════════════════════════════════════════════════════════

def analyze_emotion_and_play(frame):
    try:
        print("\n🎯 [DeepFace] 사용자의 표정을 분석합니다...", flush=True)
        small = cv2.resize(frame, (320, 240))
        result = DeepFace.analyze(img_path=small, actions=["emotion"], enforce_detection=False)
        if isinstance(result, list):
            result = result[0]
        emotion = result["dominant_emotion"].upper()
        song    = MUSIC_MAP.get(emotion, "랜덤 추천 음악")
        print(f"✨ 분석된 감정: [{emotion}] → 추천 음악: [{song}]", flush=True)
        send_to_qt(f"EMOTION_RESULT:{emotion}")
        send_to_qt(f"PLAY_SONG:{song}")
    except Exception as e:
        print(f"PYTHON_ERROR (DeepFace): {e}", flush=True)


# ═══════════════════════════════════════════════════════════════════════════════
# [스와이프] 궤적 버퍼 기반 감지기
# ═══════════════════════════════════════════════════════════════════════════════

class SwipeDetector:
    """
    최근 N 프레임의 손목 위치를 버퍼에 쌓아 스와이프를 감지한다.

    감지 조건 (모두 충족해야 트리거):
      1. 수평 이동 거리(|x_total|) > SWIPE_MIN_DIST
      2. 수평 편향: |x_total| > SWIPE_AXIS_RATIO × |y_total|  (사선 스와이프 차단)
      3. 쿨다운 경과
      4. 복귀 잠금 해제 상태

    복귀 잠금 해제 조건:
      스와이프 트리거 방향의 반대 방향으로 RETURN_UNLOCK_DIST 이상 이동하면 해제.
      손이 충분히 되돌아가야 다음 스와이프를 허용한다.
    """

    BUFFER_SIZE        = 12    # 분석할 프레임 수 (많을수록 안정, 적을수록 빠른 반응)
    SWIPE_MIN_DIST     = 0.18  # 최소 수평 이동 거리 (정규화 0~1, 줄이면 더 짧은 스와이프 인식)
    SWIPE_AXIS_RATIO   = 2.0   # 수평이 수직의 몇 배 이상이어야 스와이프로 인정
    SWIPE_COOLDOWN     = 1.2   # 연속 스와이프 최소 간격 (초)
    RETURN_UNLOCK_DIST = 0.06  # 반대 방향으로 이 거리 이상 복귀해야 잠금 해제

    def __init__(self):
        self._buf        = deque(maxlen=self.BUFFER_SIZE)
        self._last_time  = 0.0
        self._locked     = False  # 스와이프 직후 잠금 상태
        self._locked_dir = 0      # +1(NEXT) 또는 -1(PREV)
        self._prev_x     = None   # 복귀 감지용 이전 x

    def update(self, x: float, y: float):
        now = time.time()
        self._buf.append((x, y))

        # ── 복귀 잠금 해제 감시 ───────────────────────────────────────────────
        if self._locked and self._prev_x is not None:
            dx_return = x - self._prev_x
            if self._locked_dir == 1 and dx_return > self.RETURN_UNLOCK_DIST:
                self._locked = False
                print("🔓 [복귀 감지] NEXT 후 오른쪽 복귀 → 잠금 해제", flush=True)
            elif self._locked_dir == -1 and dx_return < -self.RETURN_UNLOCK_DIST:
                self._locked = False
                print("🔓 [복귀 감지] PREV 후 왼쪽 복귀 → 잠금 해제", flush=True)
        self._prev_x = x

        # ── 쿨다운 · 잠금 검사 ────────────────────────────────────────────────
        if self._locked:
            return None
        if now - self._last_time < self.SWIPE_COOLDOWN:
            return None
        if len(self._buf) < self.BUFFER_SIZE:
            return None

        # ── 버퍼 전체 이동 벡터 계산 ──────────────────────────────────────────
        xs      = [p[0] for p in self._buf]
        ys      = [p[1] for p in self._buf]
        x_total = xs[-1] - xs[0]   # 음수 = 왼쪽(NEXT), 양수 = 오른쪽(PREV)
        y_total = ys[-1] - ys[0]

        # ── 이중 조건 검사 ────────────────────────────────────────────────────
        if abs(x_total) < self.SWIPE_MIN_DIST:
            return None
        if abs(x_total) < self.SWIPE_AXIS_RATIO * abs(y_total):
            return None

        # ── 방향 결정 및 트리거 ───────────────────────────────────────────────
        if x_total < 0:
            cmd              = "NEXT"
            self._locked_dir = 1
            print(f"◀ [스와이프] NEXT  (Δx={x_total:.3f}, Δy={y_total:.3f})", flush=True)
        else:
            cmd              = "PREV"
            self._locked_dir = -1
            print(f"▶ [스와이프] PREV  (Δx={x_total:.3f}, Δy={y_total:.3f})", flush=True)

        self._last_time = now
        self._locked    = True
        self._buf.clear()   # 잔여 이동이 재트리거되지 않도록 버퍼 초기화
        return cmd

    def reset(self):
        """손이 화면에서 사라질 때 버퍼만 초기화. 잠금은 유지(복귀 판정 우선)."""
        self._buf.clear()
        self._prev_x = None


# ═══════════════════════════════════════════════════════════════════════════════
# [제스처] 백그라운드 처리 스레드
# ═══════════════════════════════════════════════════════════════════════════════

VOLUME_COOLDOWN    = 0.2
HOLD_REQUIRED_TIME = 1.5

gesture_lock     = threading.Lock()
latest_rgb_frame = None
gesture_running  = True


def gesture_processing_thread(hands):
    global latest_rgb_frame, gesture_running

    swipe = SwipeDetector()

    last_volume_time = 0.0
    prev_y           = 0.0

    start_hold_start = stop_hold_start = end_hold_start = 0.0
    start_triggered  = stop_triggered  = end_triggered  = False

    while gesture_running:
        with gesture_lock:
            local_frame = latest_rgb_frame.copy() if latest_rgb_frame is not None else None

        if local_frame is None:
            time.sleep(0.01)
            continue

        now     = time.time()
        results = hands.process(local_frame)

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                lm = hand_landmarks.landmark

                # ── 손가락 펴짐 카운트 ───────────────────────────────────────
                base_open  = [lm[tip].y < lm[pip].y for tip, pip in [(8,6),(12,10),(16,14),(20,18)]]
                four_count = sum(base_open)

                if four_count == 0:
                    open_count = 0
                elif four_count == 1:
                    open_count = 1
                else:
                    thumb_open = (lm[4].y < lm[2].y) or (abs(lm[4].x - lm[2].x) > 0.05)
                    open_count = four_count + (1 if thumb_open else 0)

                is_start = open_count == 5
                is_stop  = open_count == 1
                is_end   = open_count == 0

                # ── START (5개) ───────────────────────────────────────────────
                if is_start:
                    stop_hold_start = end_hold_start = 0.0
                    stop_triggered  = end_triggered  = False
                    if start_hold_start == 0:
                        start_hold_start = now
                    elif now - start_hold_start >= HOLD_REQUIRED_TIME and not start_triggered:
                        # ▶ 재생 중이면 START 무시 — 노래 처음부터 재시작 방지
                        with is_playing_lock:
                            playing_now = is_playing
                        if playing_now:
                            print("⚠️  [START 차단] 현재 재생 중 → 중복 START 무시", flush=True)
                        else:
                            print("✋ START", flush=True)
                            send_to_qt("START")
                        start_triggered = True

                # ── STOP (1개) ────────────────────────────────────────────────
                elif is_stop:
                    start_hold_start = end_hold_start = 0.0
                    start_triggered  = end_triggered  = False
                    if stop_hold_start == 0:
                        stop_hold_start = now
                    elif now - stop_hold_start >= HOLD_REQUIRED_TIME and not stop_triggered:
                        print("☝️ STOP", flush=True)
                        send_to_qt("STOP")
                        stop_triggered = True

                # ── END (0개 / 주먹) ──────────────────────────────────────────
                elif is_end:
                    start_hold_start = stop_hold_start = 0.0
                    start_triggered  = stop_triggered  = False
                    if end_hold_start == 0:
                        end_hold_start = now
                    elif now - end_hold_start >= HOLD_REQUIRED_TIME and not end_triggered:
                        print("✊ END", flush=True)
                        send_to_qt("END")
                        end_triggered = True

                else:
                    start_hold_start = stop_hold_start = end_hold_start = 0.0
                    start_triggered  = stop_triggered  = end_triggered  = False

                # ── 볼륨 (수직 이동) ─────────────────────────────────────────
                cur_y = lm[0].y
                if prev_y != 0 and now - last_volume_time > VOLUME_COOLDOWN:
                    dy = cur_y - prev_y
                    if dy < -0.08:
                        print("▲ VOLUME_UP", flush=True)
                        send_to_qt("VOLUME_UP")
                        last_volume_time = now
                    elif dy > 0.08:
                        print("▼ VOLUME_DOWN", flush=True)
                        send_to_qt("VOLUME_DOWN")
                        last_volume_time = now
                prev_y = cur_y

                # ── 스와이프 (수평 이동) ─────────────────────────────────────
                cmd = swipe.update(lm[0].x, lm[0].y)
                if cmd:
                    send_to_qt(cmd)

        else:
            # 손이 화면에서 사라짐
            prev_y           = 0.0
            start_hold_start = stop_hold_start = end_hold_start = 0.0
            start_triggered  = stop_triggered  = end_triggered  = False
            swipe.reset()

        time.sleep(0.02)


# ═══════════════════════════════════════════════════════════════════════════════
# [메인] 카메라 루프
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    global latest_rgb_frame, gesture_running

    threading.Thread(target=connect_to_qt, daemon=True).start()

    mp_hands = mp.solutions.hands
    hands    = mp_hands.Hands(
        max_num_hands=1,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5,
    )

    cap = None
    for idx in range(10):
        print(f"📷 카메라 인덱스 [{idx}]번 오픈 시도 중...", flush=True)
        cap = cv2.VideoCapture(idx, cv2.CAP_V4L2)
        if cap.isOpened():
            print(f"✅ 카메라 [{idx}]번 장치를 성공적으로 열었습니다!", flush=True)
            break
        cap.release()

    if not cap or not cap.isOpened():
        print("\n❌ PYTHON_ERROR: 모든 인덱스에서 카메라를 열 수 없습니다.", flush=True)
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_BUFFERSIZE,   1)

    initial_scan_done = False
    scan_start_time   = 0.0

    threading.Thread(target=gesture_processing_thread, args=(hands,), daemon=True).start()

    print("\n🚀 [제스처/감정 AI 엔진] 가동 완료. 스트리밍 시작...")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.005)
            continue

        frame     = cv2.flip(frame, 1)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        with gesture_lock:
            latest_rgb_frame = rgb_frame

        now = time.time()

        if qt_connected and not initial_scan_done:
            if scan_start_time == 0:
                scan_start_time = now
                print("\n⏳ Qt 연동 완료! 5초 후 표정을 스캔합니다...", flush=True)
            elif now - scan_start_time >= 5.0:
                print("📸 찰칵! 표정 데이터를 캡처했습니다.", flush=True)
                threading.Thread(
                    target=analyze_emotion_and_play,
                    args=(frame.copy(),),
                    daemon=True,
                ).start()
                initial_scan_done = True

        if cv2.waitKey(10) & 0xFF == ord("q"):
            print("\n⏹️ 사용자에 의해 카메라 모니터링이 종료되었습니다.", flush=True)
            break

    gesture_running = False
    if qt_socket:
        qt_socket.close()
    cap.release()


if __name__ == "__main__":
    main()
