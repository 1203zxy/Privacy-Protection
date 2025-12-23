from flask import Flask, Response
import cv2
import atexit
import time

app = Flask(__name__)
camera = cv2.VideoCapture(0)

# 设置摄像头参数
camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
camera.set(cv2.CAP_PROP_FPS, 30)

if not camera.isOpened():
    raise RuntimeError("Failed to open webcam")

@atexit.register
def cleanup():
    print("Releasing camera")
    camera.release()

def generate_frames():
    while True:
        success, frame = camera.read()
        if not success:
            time.sleep(0.1)
            continue
        
        ret, buffer = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        if not ret:
            continue
        
        frame_bytes = buffer.tobytes()
        
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n'
               b'Content-Length: ' + str(len(frame_bytes)).encode() + b'\r\n'
               b'\r\n' + frame_bytes + b'\r\n')

@app.route('/video')
def video():
    return Response(generate_frames(), 
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/health')
def health():
    return "OK"

if __name__ == '__main__':
    print("=" * 50)
    print("Webcam Server Started!")
    print("Stream URL: http://0.0.0.0:5000/video")
    print("Single frame: http://0.0.0.0:5000/frame")
    print("=" * 50)
    app.run(host='0.0.0.0', port=5000, threaded=True)