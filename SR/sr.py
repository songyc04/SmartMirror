# test_sr.py
import speech_recognition as sr

LANGUAGE     = "ko-KR"
DEVICE_INDEX = 10

def main():
    r   = sr.Recognizer()
    mic = sr.Microphone(device_index=DEVICE_INDEX)

    print(f"[마이크] {sr.Microphone.list_microphone_names()[DEVICE_INDEX]}")
    print("[1] 주변 소음 감지 중... (잠시 조용히 해주세요)")
    with mic as source:
        r.adjust_for_ambient_noise(source, duration=2)
        print("[2] 음성인식 시작 — 말씀하세요. (Ctrl+C 로 종료)")
        print("-" * 40)

        while True:
            try:
                audio = r.listen(source, timeout=5, phrase_time_limit=5)
                print("[인식 중...]")
                text = r.recognize_google(audio, language=LANGUAGE)
                print(f"[인식 완료] {text}")

            except sr.WaitTimeoutError:
                print("[대기 중...]", end="\r")
            except sr.UnknownValueError:
                print("[인식 실패] 다시 말씀해주세요.")
            except sr.RequestError as e:
                print(f"[서버 오류] {e}")
            except KeyboardInterrupt:
                print("\n[종료]")
                break

if __name__ == "__main__":
    main()
