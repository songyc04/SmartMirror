# voice_controller.py
import speech_recognition as sr

LANGUAGE     = "ko-KR"
DEVICE_INDEX = 10       # FF Camera: USB Audio
WAKE_WORD    = "헤이 미러"
COMMAND_TIMEOUT = 7     # Wake word 감지 후 명령 대기 시간 (초)

# 명령어 → 아두이노 전송 문자열 매핑
COMMANDS = {
    "차 시동": "CAR_START",
    "차 꺼줘": "CAR_STOP",
    "불 켜줘": "LIGHT_ON",
    "불 꺼줘": "LIGHT_OFF",
}

def listen_once(recognizer, mic, timeout=5, phrase_limit=5) -> str | None:
    """마이크에서 음성 한 번 듣고 텍스트 반환, 실패 시 None"""
    try:
        audio = recognizer.listen(
            mic,
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
    """인식된 텍스트에서 명령어 매칭"""
    for keyword, action in COMMANDS.items():
        if keyword in text:
            return action
    return None

def main():
    r   = sr.Recognizer()
    mic = sr.Microphone(device_index=DEVICE_INDEX)

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
