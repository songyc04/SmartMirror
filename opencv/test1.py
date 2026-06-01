import sys
import cv2
from deepface import DeepFace

def analyze_emotion():
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
            print(f"[{i+1}/10] EMOTION_RESULT:{dominant_emotion}", flush=True)

        except Exception as e:
            print(f"PYTHON_ERROR: {str(e)}", flush=True)

    cap.release()
    print("완료")

if __name__ == "__main__":
    analyze_emotion()
