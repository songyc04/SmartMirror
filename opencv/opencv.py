import sys
import socket  # 추가: UDP 소켓 통신 라이브러리
import cv2
from deepface import DeepFace

# ── UDP 네트워크 설정 ──────────────────────────
UDP_IP = "127.0.0.1"  # Qt가 실행 중인 로컬 호스트 IP
UDP_PORT = 9001       # 수신할 Qt의 포트 번호
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def analyze_emotion():
    # 리눅스 환경 카메라 백엔드 유지
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    if not cap.isOpened():
        print("PYTHON_ERROR: Cannot open camera")
        sys.exit(1)

    print("Python DeepFace: 표정 분석 중...", flush=True)

    for i in range(10):
        ret, frame = cap.read()
        if not ret:
            break

        try:
            analysis = DeepFace.analyze(
                img_path=frame,
                actions=['emotion'],
                enforce_detection=False
            )
            if isinstance(analysis, list):
                analysis = analysis[0]
            dominant_emotion = analysis["dominant_emotion"]
            
            # 1. 감정 문자열을 대문자로 변환 (예: happy -> HAPPY)
            upper_emotion = dominant_emotion.upper()
            print(f"[{i+1}/10] EMOTION_RESULT:{upper_emotion}", flush=True)

            # 2. Qt(QUdpSocket)로 대문자 데이터 전송
            sock.sendto(upper_emotion.encode('utf-8'), (UDP_IP, UDP_PORT))

        except Exception as e:
            print(f"PYTHON_ERROR: {str(e)}", flush=True)

    cap.release()
    print("완료")

if __name__ == "__main__":
    analyze_emotion()
