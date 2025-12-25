# 实时隐私保护工具

## 项目简介
随着摄像头的快速普及和人脸识别技术的广泛应用，个人隐私面临严重泄露风险。人脸作为独特的生物特征，一旦被不当获取，可能导致身份盗用、跟踪等安全问题。

本项目开发了一款高效的实时隐私保护工具，使用 C++ 和 OpenCV，通过摄像头捕捉视频流，实时检测人脸并提供三种隐私保护模式：  
- **模糊处理 (Blur)**  
- **像素化处理 (Pixel)**  
- **遮罩覆盖 (Mask)**  

用户可根据需求动态切换模式、调整参数强度，并在运行时上传自定义遮罩图片，实现个性化隐私保护。

## 环境要求与依赖

- OpenCV 4.10 或更高版本（需包含 DNN 模块以支持 YuNet）
- 人脸检测模型：`face_detection_yunet_2023mar.onnx`（已包含在项目中）
- 遮罩图片：存放在 `mask/` 文件夹中，支持透明 PNG（alpha 通道）以获得更好效果
- 编译工具：CMake + GCC/Clang

**推荐环境**：Ubuntu / WSL2 + OpenCV 4.12（从源编译）<br>
  - WSL 用户：Windows 运行 `python flask/webcam_server.py`（需 pip install flask opencv-python），然后用 --stream 指定。
## 编译与运行

```bash
# 编译
cmake . && make

# 运行示例
./privacy_protector                                      # 默认：blur 模式，使用本地摄像头
./privacy_protector --mode pixel --pixel_size 20
./privacy_protector --mode mask --mask_image mask1.png
./privacy_protector --device 1                           # 指定本地摄像头设备
./privacy_protector --stream http://your-ip:5000/video   # WSL 用户使用流（优先于 --device）
````

## 命令行参数
| 参数          | 说明                                      | 默认值               |
|---------------|-------------------------------------------|----------------------|
| --mode        | 初始保护模式：可选值为 blur、pixel、mask   | blur                 |
| --blur_size   | 模糊核大小（需为奇数，数值越大模糊效果越强） | 51                   |
| --pixel_size  | 像素块大小（数值越大像素化效果越明显）    | 15                   |
| --mask_image  | 遮罩图片文件名（图片需放在 mask/ 文件夹下） | default_mask.png     |
| --device      | 本地摄像头设备编号                       | 0                  |
| --stream      | HTTP 视频流地址（指定后优先于本地摄像头）  | 无（默认用本地摄像头） |

## 运行时交互操作

- 按键'1' → 切换到模糊模式
- 按键'2' → 切换到像素化模式
- 按键'3' → 切换到遮罩模式
- 按键'[' → 减小当前模式参数强度
- 按键']' → 增大当前模式参数强度
- 按键'U'(或'u')→ 暂停视频，输入新的遮罩文件名（仅输入文件名，如 emoji.png），程序自动从 mask/ 文件夹加载
- 按键'ESC' → 退出程序

窗口左上角实时显示当前模式、参数值及检测到的人脸数量。

## 功能实现细节

实时人脸检测：使用 OpenCV Zoo 提供的 YuNet 模型，置信度阈值 0.6，NMS 阈值 0.3，尽可能检测所有人脸。<br>
模糊模式：cv::GaussianBlur，支持动态调整核大小。<br>
像素化模式：双重 cv::resize（INTER_NEAREST），支持动态调整块大小。<br>
遮罩模式：自动缩放 + alpha 混合，支持透明 PNG，运行时可更换图片。<br>
参考：OpenCV Face Detection: Cascade Classifier vs. YuNet<br>
