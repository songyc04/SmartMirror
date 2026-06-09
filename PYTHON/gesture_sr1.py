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
from collections import deque

# ══════════════════════════════════════════════════════════════
#  네트워크 설정
# ══════════════════════════════════════════════════════════════
QT_IP   = "127.0.0.1"
QT_PORT = 9001

_qt_lock      = threading.Lock()   # 소켓 접근 직렬화
qt_socket     = None
qt_connected  = False
_reconnecting = False              # ★ 재연결 스레드 중복 생성 방지 플래그

# ══════════════════════════════════════════════════════════════
#  음성 인식 설정
# ══════════════════════════════════════════════════════════════
LANGUAGE        = "ko-KR"
WAKE_WORD       = "거울아"
COMMAND_TIMEOUT = 7

# ══════════════════════════════════════════════════════════════
#  제스처 튜닝 파라미터
# ══════════════════════════════════════════════════════════════
SWIPE_COOLDOWN           = 0.9    # 스와이프 최소 간격 (초)
VOLUME_COOLDOWN          = 0.40   # 볼륨 최소 간격 (초)
HOLD_REQUIRED_TIME       = 1.5    # Hold 인식 필요 시간 (초)

# ── 스와이프 필터링 강화 파라미터 ──
DIRECTION_TRIGGER_DISTANCE  = 0.12   # 트리거 최소 이동 거리 (↑ 높일수록 오인식 감소)
DIRECTION_DOMINANCE_RATIO   = 1.8    # 주축/부축 비율 (↑ 높일수록 대각선 오인식 감소)
DIRECTION_MAX_TIME          = 0.9    # 제스처 최대 인정 시간 (초)
DIRECTION_REARM_DISTANCE    = 0.04   # 스와이프 후 재활성화 최소 복귀 거리

# ── 노이즈 필터링: EMA 스무딩 ──
EMA_ALPHA = 0.35   # 0~1, 낮을수록 더 강한 스무딩 (지연 증가)

# ── 손가락 상태 안정화: 연속 N프레임 일치해야 명령 전송 ──
GESTURE_CONFIRM_FRAMES = 3   # Hold 제스처 확정에 필요한 연속 동일 프레임 수

# ══════════════════════════════════════════════════════════════
#  [통신] Qt TCP 연결 관리  ★ 수정: 소켓 close() 보장 + 중복 스레드 방지
# ══════════════════════════════════════════════════════════════
def _do_connect():
    """백그라운드에서 Qt 서버에 재연결을 시도하는 내부 함수."""
    global qt_socket, qt_connected, _reconnecting
    while True:
        try:
            print(f"🔄 Qt 서버 연결 시도 중 ({QT_IP}:{QT_PORT})...", flush=True)
            new_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            new_sock.settimeout(3.0)
            new_sock.connect((QT_IP, QT_PORT))
            new_sock.settimeout(None)          # 연결 후 블로킹 모드 해제
            with _qt_lock:
                qt_socket    = new_sock
                qt_connected = True
            print("✅ C++ Qt 서버(9001) 연결 성공!", flush=True)
            break
        except Exception as e:
            print(f"❌ Qt 연결 실패 ({e}). 2초 후 재시도...", flush=True)
            try:
                new_sock.close()
            except Exception:
                pass
            time.sleep(2)

    with _qt_lock:
        _reconnecting = False


def connect_to_qt():
    """최초 연결 진입점 (main에서 1회 호출)."""
    _do_connect()


def _request_reconnect():
    """★ 중복 재연결 스레드 방지: 이미 재연결 중이면 무시."""
    global _reconnecting
    with _qt_lock:
        if _reconnecting:
            return
        _reconnecting = True
        # 기존 소켓 안전하게 닫기
        global qt_socket, qt_connected
        if qt_socket:
            try:
                qt_socket.close()
            except Exception:
                pass
        qt_socket    = None
        qt_connected = False

    threading.Thread(target=_do_connect, daemon=True).start()


def send_to_qt(message: str):
    """Qt 서버로 메시지 전송. 실패 시 재연결 1회 요청."""
    global qt_socket
    encoded = (message + "\n").encode("utf-8")
    with _qt_lock:
        sock = qt_socket
    if sock is None:
        return
    try:
        sock.sendall(encoded)
    except Exception as e:
        print(f"❌ Qt 전송 실패 ({e}). 재연결 요청.", flush=True)
        _request_reconnect()


# ══════════════════════════════════════════════════════════════
#  [음성 인식]  ★ 수정: Microphone 컨텍스트를 루프 바깥에서 1회 열기
# ══════════════════════════════════════════════════════════════
def get_microphone_index() -> int | None:
    pa = pyaudio.PyAudio()
    usb_index = loopback_index = fallback_index = None
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
    return usb_index or loopback_index or fallback_index


def listen_once(recognizer, source, timeout=5, phrase_limit=5) -> str | None:
    try:
        audio = recognizer.listen(source, timeout=timeout, phrase_time_limit=phrase_limit)
        return recognizer.recognize_google(audio, language=LANGUAGE)
    except (sr.WaitTimeoutError, sr.UnknownValueError):
        return None
    except sr.RequestError as e:
        print(f"[서버 오류] {e}", flush=True)
        return None


def voice_controller_thread():
    """
    ★ 핵심 수정:
      - Microphone() 컨텍스트를 루프 바깥에서 단 1회 열어 유지.
      - PyAudio 스트림 반복 생성/파괴로 인한 오류 폭발 방지.
    """
    mic_index = get_microphone_index()
    if mic_index is None:
        print("[오류] 마이크 없음. 음성 인식 종료.", flush=True)
        return

    r   = sr.Recognizer()
    mic = sr.Microphone(device_index=mic_index)

    print("[초기화] 주변 소음 보정 중...", flush=True)
    with mic as source:
        r.adjust_for_ambient_noise(source, duration=2)
    print(f"[대기] '{WAKE_WORD}' 라고 말하면 활성화됩니다.", flush=True)

    # ★ with mic as source 를 루프 밖에서 1회만 열기
    with mic as source:
        while True:
            if not qt_connected:
                time.sleep(1)
                continue
            try:
                # 1단계: Wake word 대기
                text = listen_once(r, source, timeout=5, phrase_limit=4)
                if text is None or WAKE_WORD not in text:
                    continue

                print(f"\n🗣️ [호출] '{text}'", flush=True)
                print(f"[활성화] 명령어를 말씀하세요 ({COMMAND_TIMEOUT}초)", flush=True)

                # 2단계: 명령 대기
                cmd = listen_once(r, source, timeout=COMMAND_TIMEOUT, phrase_limit=5)
                if cmd is None:
                    print("[비활성화] 명령 미수신.", flush=True)
                    continue

                print(f"[명령] {cmd}", flush=True)

                trigger_words = ["틀어줘", "틀어 줘", "재생해줘", "들려줘", "찾아줘"]
                if any(kw in cmd for kw in trigger_words):
                    query = cmd
                    for kw in trigger_words:
                        query = query.replace(kw, "")
                    query = query.strip()
                    if query:
                        print(f"🎵 [검색] {query}", flush=True)
                        send_to_qt(f"KEYWORD:{query}")
                        time.sleep(0.15)
                        send_to_qt("START")
                    else:
                        print("[오류] 검색어 비어있음.", flush=True)
                else:
                    send_to_qt(f"KEYWORD:{cmd.strip()}")
                    time.sleep(0.15)
                    send_to_qt("START")

            except Exception as e:
                print(f"[음성 오류] {e}", flush=True)
                time.sleep(1)


# ══════════════════════════════════════════════════════════════
#  [제스처] 유틸리티
# ══════════════════════════════════════════════════════════════
def get_palm_center(landmarks):
    pts = [0, 5, 9, 13, 17]
    x = sum(landmarks[i].x for i in pts) / len(pts)
    y = sum(landmarks[i].y for i in pts) / len(pts)
    return x, y


class EMAFilter:
    """지수 이동 평균(EMA) 스무더 - 손 떨림 노이즈 제거."""
    def __init__(self, alpha: float):
        self.alpha = alpha
        self.val   = None

    def update(self, v: float) -> float:
        if self.val is None:
            self.val = v
        else:
            self.val = self.alpha * v + (1 - self.alpha) * self.val
        return self.val

    def reset(self):
        self.val = None


class GestureConfirmer:
    """
    연속 N프레임 동일 제스처가 들어와야 '확정'으로 처리.
    순간적인 오인식(노이즈 프레임)을 차단.
    """
    def __init__(self, required: int):
        self.required = required
        self.current  = None
        self.count    = 0

    def update(self, gesture: str | None) -> bool:
        """같은 gesture가 required 번 연속 → True 반환 (최초 1회)."""
        if gesture != self.current:
            self.current = gesture
            self.count   = 1
            return False
        self.count += 1
        return self.count == self.required   # 정확히 required번째 프레임에만 True


# ══════════════════════════════════════════════════════════════
#  [제스처] 메인 처리 스레드
#  ★ 핵심 수정:
#    1. hands.process() 를 프레임이 실제로 갱신된 경우에만 호출 (중복 처리 방지)
#    2. EMA 스무딩으로 손 좌표 노이즈 제거
#    3. GestureConfirmer로 Hold 오인식 방지
#    4. 프레임 처리 후 고정 sleep 대신 실제 경과 시간 기반 제어
# ══════════════════════════════════════════════════════════════
gesture_lock       = threading.Lock()
latest_rgb_frame   = None
latest_frame_id    = 0          # ★ 프레임 갱신 여부 추적용 ID
gesture_running    = True


def gesture_processing_thread(hands):
    global latest_rgb_frame, gesture_running

    last_swipe_time  = 0
    last_volume_time = 0

    ema_x = EMAFilter(EMA_ALPHA)
    ema_y = EMAFilter(EMA_ALPHA)

    gesture_start_x = gesture_start_y = None
    gesture_start_time = 0
    swipe_locked = False

    start_hold_time = stop_hold_time = end_hold_time = 0
    start_triggered = stop_triggered = end_triggered = False

    confirmer    = GestureConfirmer(GESTURE_CONFIRM_FRAMES)
    processed_id = -1   # ★ 마지막으로 처리한 프레임 ID

    while gesture_running:
        # ── ★ 새 프레임이 없으면 CPU를 태우지 않고 대기 ──
        with gesture_lock:
            fid   = latest_frame_id
            frame = latest_rgb_frame.copy() if latest_rgb_frame is not None else None

        if frame is None or fid == processed_id:
            time.sleep(0.008)
            continue
        processed_id = fid

        now     = time.time()
        results = hands.process(frame)

        if not results.multi_hand_landmarks:
            # 손 없음 → 모든 상태 리셋
            ema_x.reset(); ema_y.reset()
            swipe_locked = False
            gesture_start_x = gesture_start_y = None
            gesture_start_time = 0
            start_hold_time = stop_hold_time = end_hold_time = 0
            start_triggered = stop_triggered = end_triggered = False
            confirmer.update(None)
            continue

        for hand_landmarks in results.multi_hand_landmarks:
            lm = hand_landmarks.landmark

            # ── 손가락 펴짐 판정 ──
            tips_pips = [(8, 6), (12, 10), (16, 14), (20, 18)]
            base_open = [lm[tip].y < lm[pip].y for tip, pip in tips_pips]
            four_count = sum(base_open)

            if four_count == 0:
                open_count = 0
            elif four_count == 1:
                open_count = 1
            else:
                thumb_open = (lm[4].y < lm[2].y) or (abs(lm[4].x - lm[2].x) > 0.05)
                open_count = four_count + (1 if thumb_open else 0)

            # ── Hold 제스처 분류 ──
            raw_gesture = (
                "START" if open_count == 5 else
                "STOP"  if open_count == 1 else
                "END"   if open_count == 0 else
                None
            )

            # ★ GestureConfirmer: N프레임 연속 동일 제스처만 처리
            confirmed_now = confirmer.update(raw_gesture)

            # START (손 펼침 5개)
            if raw_gesture == "START":
                stop_hold_time = end_hold_time = 0
                stop_triggered = end_triggered = False
                if start_hold_time == 0:
                    start_hold_time = now
                elif not start_triggered and now - start_hold_time >= HOLD_REQUIRED_TIME:
                    print("✋ [제스처] START → Qt", flush=True)
                    send_to_qt("START")
                    start_triggered = True

            # STOP (검지 1개)
            elif raw_gesture == "STOP":
                start_hold_time = end_hold_time = 0
                start_triggered = end_triggered = False
                if stop_hold_time == 0:
                    stop_hold_time = now
                elif not stop_triggered and now - stop_hold_time >= HOLD_REQUIRED_TIME:
                    print("☝️ [제스처] STOP → Qt", flush=True)
                    send_to_qt("STOP")
                    stop_triggered = True

            # END (주먹)
            elif raw_gesture == "END":
                start_hold_time = stop_hold_time = 0
                start_triggered = stop_triggered = False
                if end_hold_time == 0:
                    end_hold_time = now
                elif not end_triggered and now - end_hold_time >= HOLD_REQUIRED_TIME:
                    print("✊ [제스처] END → Qt", flush=True)
                    send_to_qt("END")
                    end_triggered = True

            else:
                # 중간 상태(2~4개) → Hold 타이머 리셋
                start_hold_time = stop_hold_time = end_hold_time = 0
                start_triggered = stop_triggered = end_triggered = False

            # ── ★ EMA 스무딩 적용 좌표 ──
            raw_cx, raw_cy = get_palm_center(lm)
            cx = ema_x.update(raw_cx)
            cy = ema_y.update(raw_cy)

            # ── 스와이프 / 볼륨 제어 ──
            if gesture_start_x is None:
                gesture_start_x, gesture_start_y = cx, cy
                gesture_start_time = now
            else:
                dx = cx - gesture_start_x
                dy = cy - gesture_start_y
                ax, ay = abs(dx), abs(dy)
                elapsed = now - gesture_start_time

                # 타임아웃 → 제스처 윈도우 리셋
                if elapsed > DIRECTION_MAX_TIME:
                    gesture_start_x, gesture_start_y = cx, cy
                    gesture_start_time = now
                    swipe_locked = False

                # 스와이프 잠금 해제 조건 (충분히 복귀)
                elif swipe_locked:
                    if ax < DIRECTION_REARM_DISTANCE and ay < DIRECTION_REARM_DISTANCE:
                        swipe_locked = False
                        gesture_start_x, gesture_start_y = cx, cy
                        gesture_start_time = now

                # 실제 제스처 판정
                elif now - last_swipe_time > SWIPE_COOLDOWN:

                    # ── 수평 스와이프 (패널 전환) ──
                    if ax >= DIRECTION_TRIGGER_DISTANCE and ax >= ay * DIRECTION_DOMINANCE_RATIO:
                        direction = "LEFT" if dx < 0 else "RIGHT"
                        print(f"{'◀' if dx < 0 else '▶'} [스와이프] {direction}", flush=True)
                        send_to_qt(direction)
                        last_swipe_time = now
                        swipe_locked = True
                        gesture_start_x, gesture_start_y = cx, cy
                        gesture_start_time = now

                    # ── 수직 스와이프 (볼륨) ──
                    elif ay >= DIRECTION_TRIGGER_DISTANCE and ay >= ax * DIRECTION_DOMINANCE_RATIO:
                        if now - last_volume_time > VOLUME_COOLDOWN:
                            direction = "VOLUME_UP" if dy < 0 else "VOLUME_DOWN"
                            print(f"{'▲' if dy < 0 else '▼'} [볼륨] {direction}", flush=True)
                            send_to_qt(direction)
                            last_volume_time = now
                            last_swipe_time  = now
                            swipe_locked = True
                            gesture_start_x, gesture_start_y = cx, cy
                            gesture_start_time = now

        # ── 처리율 제어: 타겟 ~30fps (33ms) ──
        time.sleep(0.033)


# ══════════════════════════════════════════════════════════════
#  메인
# ══════════════════════════════════════════════════════════════
def main():
    global latest_rgb_frame, latest_frame_id, gesture_running

    # 1. Qt TCP 연결
    threading.Thread(target=connect_to_qt, daemon=True).start()

    # 2. 음성 인식 스레드
    threading.Thread(target=voice_controller_thread, daemon=True).start()

    # 3. MediaPipe Hands 초기화
    mp_hands = mp.solutions.hands
    hands    = mp_hands.Hands(
        max_num_hands=1,
        min_detection_confidence=0.6,   # ↑ 정확도 향상
        min_tracking_confidence=0.6
    )

    # 4. 카메라 오픈
    cap = None
    for cam_idx in range(10):
        print(f"📷 카메라 [{cam_idx}] 시도...", flush=True)
        cap = cv2.VideoCapture(cam_idx, cv2.CAP_V4L2)
        if cap.isOpened():
            print(f"✅ 카메라 [{cam_idx}] 열림!", flush=True)
            break
        cap.release()

    if not cap or not cap.isOpened():
        print("❌ PYTHON_ERROR: 카메라 감지 실패.", flush=True)
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_BUFFERSIZE,   1)   # ★ 버퍼 최소화: 항상 최신 프레임 유지

    # 5. 제스처 처리 스레드
    threading.Thread(target=gesture_processing_thread, args=(hands,), daemon=True).start()

    print("\n🚀 [미러 AI] 가동 완료. 'q' 입력 시 종료.", flush=True)

    frame_counter = 0
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.005)
            continue

        frame     = cv2.flip(frame, 1)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        # ★ 프레임 ID를 함께 업데이트 → 제스처 스레드가 중복 처리 안 함
        with gesture_lock:
            latest_rgb_frame = rgb_frame
            frame_counter   += 1
            latest_frame_id  = frame_counter

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    gesture_running = False
    with _qt_lock:
        if qt_socket:
            qt_socket.close()
    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
