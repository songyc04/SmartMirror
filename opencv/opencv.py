import sys
import socket  # TCP 소켓 통信 라이브러리
import cv2
from deepface import DeepFace

# ── TCP/IP 네트워크 설정 ──────────────────────────
TCP_IP = "127.0.0.1"  # Qt가 실행 중인 로컬 호스트 IP
TCP_PORT = 9001       # 연결할 Qt의 TCP 포트 번호


def analyze_emotion():
   finalData = ""
   # 리눅스 환경 카메라 백엔드 유지
   # cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
   cap = cv2.VideoCapture(0)
   if not cap.isOpened():
      print("PYTHON_ERROR: Cannot open camera")
      sys.exit(1)

   # 1. TCP 소켓 생성 (SOCK_STREAM)
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   
   try:
      print(f"Qt TCP 서버({TCP_IP}:{TCP_PORT})에 연결 시도 중...", flush=True)
      sock.connect((TCP_IP, TCP_PORT)) # TCP는 데이터를 보내기 전 서버에 먼저 연결해야 합니다.
      print("Qt TCP 서버에 성공적으로 연결되었습니다.", flush=True)
   except Exception as e:
      print(f"PYTHON_ERROR: TCP 연결 실패 - {str(e)}")
      cap.release()
      sys.exit(1)

   print("Python DeepFace: 표정 분석 중...", flush=True)

   for i in range(5):
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
         
         # 감정 문자열을 대문자로 변환 (예: happy -> HAPPY)
         upper_emotion = dominant_emotion.upper()
         print(f"[{i+1}/10] EMOTION_RESULT:{upper_emotion}", flush=True)

      except Exception as e:
         print(f"PYTHON_ERROR: {str(e)}", flush=True)
             
         
      finalData = upper_emotion
   # Qt로 TCP 데이터 전송
   # TCP 소켓은 연결이 수립되어 있으므로 대상 IP/포트 지정 없이 send()를 사용합니다.
   # Qt에서 라인 단위(canReadLine())로 읽기 쉽게 끝에 줄바꿈(\n)을 추가합니다.
   # data_to_send = f"{finalData}\n"
   sock.sendall(f"{finalData}\n".encode('utf-8'))

   # 사용이 끝난 자원 해제
   cap.release()
   sock.close() # TCP 연결 종료
   print("완료")

if __name__ == "__main__":
   analyze_emotion()
