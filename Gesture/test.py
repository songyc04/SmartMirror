import sys
import cv2
from deepface import DeepFace

def analyze_emotion():
    # 1. 카메라 열기
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    if not cap.isOpened():
        print("PYTHON_ERROR: Cannot open camera")
        sys.exit(1)
        
    print("Python DeepFace: 표정 분석 중...", flush=True)
    ret, frame = cap.read()
    cap.release()
    
    if not ret:
        print("PYTHON_ERROR: Frame empty")
        sys.exit(1)
        
    try:
        # 2. DeepFace.analyze를 사용하여 얼굴의 감정(actions=['emotion']) 분석
        # 젯슨 보드의 연산 속도 향상을 위해 enforce_detection=False 설정
        analysis = DeepFace.analyze(img_path=frame, actions=['emotion'], enforce_detection=False)
        
        # 결과가 리스트 형태로 반환되므로 첫 번째 요소 선택
        if isinstance(analysis, list):
            analysis = analysis[0]
            
        dominant_emotion = analysis["dominant_emotion"]
        
        # 3. 분석된 주 감정을 C++이 읽을 수 있도록 표준 출력
        print(f"EMOTION_RESULT:{dominant_emotion}", flush=True)
            
    except Exception as e:
        print(f"PYTHON_ERROR: {str(e)}", flush=True)

if __name__ == "__main__":
    analyze_emotion()
