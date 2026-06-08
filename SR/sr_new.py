# voice_controller.py
import speech_recognition as sr
import pyaudio

LANGUAGE        = "ko-KR"
WAKE_WORD       = "헤이 미러"
COMMAND_TIMEOUT = 7  # Wake word 감지 후 명령 대기 시간 (초)

COMMANDS = {
    "차 시동": "CAR_START",
    "차 꺼줘": "CAR_STOP",
    "불 켜줘": "LIGHT_ON",
    "불 꺼줘": "LIGHT_OFF",
}


def get_microphone_index() -> int | None:
    pa = pyaudio.PyAudio()
    usb_index      = None
    loopback_index = None
    fallback_index = None

    print("[마이크 목록]")
    for i in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(i)
        if info["maxInputChannels"] > 0:
            name = info["name"]
            print(f"  {i}번: {name}")
            if ("USB" in name or "Camera" in name) and usb_index is None:
                usb_index = i
            if "Loopback" in name and loopback_index is None:
                loopback_index = i
            if fallback_index is None:
                fallback_index = i

    pa.terminate()

    if usb_index is not None:
        print(f"[마이크 자동 감지] USB 카메라 마이크 {usb_index}번 사용")
        return usb_index
    if loopback_index is not None:
        print(f"[마이크 자동 감지] Loopback {loopback_index}번 사용")
        return loopback_index

    print(f"[마이크 자동 감지] {fallback_index}번 사용 (기본값)")
    return fallback_index


def listen_once(recognizer, source, timeout=5, phrase_limit=5) -> str | None:
    try:
        audio = recognizer.listen(
            source,
            timeout=timeout,
            phrase_time_limit=phrase_limit
        )
        text = recognizer.recognize_google(audio, language=LANGUAGE)
        return text
    except sr.WaitTimeoutError:
        return None
    except sr.UnknownValueError:
        return None
    except sr.RequestError as e:
        print(f"[서버 오류] {e}")
        return None


def match_command(text: str) -> str | None:
    for keyword, action in COMMANDS.items():
        if keyword in text:
            return action
    return None


def main():
    mic_index = get_microphone_index()
    if mic_index is None:
        print("[오류] 사용 가능한 마이크가 없습니다.")
        return

    r   = sr.Recognizer()
    mic = sr.Microphone(device_index=mic_index)

    print("[초기화] 주변 소음 감지 중... (잠시 조용히 해주세요)")
    with mic as source:
        r.adjust_for_ambient_noise(source, duration=2)

    print(f"[대기 중] '{WAKE_WORD}' 라고 말하면 활성화됩니다.")
    print("-" * 40)

    while True:
        try:
            with mic as source:
                # ── 1단계: Wake word 대기 ──────────────
                text = listen_once(r, source, timeout=5, phrase_limit=4)

                if text is None:
                    print("[대기 중...]", end="\r")
                    continue

                print(f"[수신] {text}")

                if WAKE_WORD not in text:
                    continue

                # ── 2단계: 명령어 대기 ────────────────
                print(f"[활성화] 명령을 말씀하세요. ({COMMAND_TIMEOUT}초 내)")

                command_text = listen_once(
                    r, source,
                    timeout=COMMAND_TIMEOUT,
                    phrase_limit=5
                )

                if command_text is None:
                    print("[비활성화] 명령을 듣지 못했습니다.")
                    print(f"[대기 중] '{WAKE_WORD}' 라고 말하면 활성화됩니다.")
                    print("-" * 40)
                    continue

                print(f"[명령 수신] {command_text}")

                action = match_command(command_text)
                if action:
                    print(f"[실행] {action}")
                    # send_to_arduino(action)  ← 아두이노 연동 시 주석 해제
                else:
                    print(f"[알 수 없는 명령] '{command_text}'")

                print(f"[대기 중] '{WAKE_WORD}' 라고 말하면 활성화됩니다.")
                print("-" * 40)

        except KeyboardInterrupt:
            print("\n[종료] 음성인식 종료.")
            break


if __name__ == "__main__":
    main()
