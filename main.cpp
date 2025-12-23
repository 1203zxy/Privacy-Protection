#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    // 定义参数格式（增大默认模糊值）
    CommandLineParser parser(argc, argv, 
        "{help h||Show help message}"
        "{mode|blur|Mode: blur, pixel, mask}"
        "{blur_size|51|Blur kernel size (odd number, larger=more blur)}"
        "{pixel_size|15|Pixel block size (larger=more pixelated)}"
        "{mask_image|default_mask.png|Path to mask image}"
        "{device|0|Camera device number}"
        "{stream||HTTP stream URL (overrides device)}"
    );

    // 显示帮助
    if (parser.has("help")) {
        cout << "Usage: ./privacy_protector [options]" << endl;
        cout << "Options:" << endl;
        cout << "  --mode=blur|pixel|mask   Protection mode" << endl;
        cout << "  --blur_size=N            Blur strength (odd, default 51)" << endl;
        cout << "  --pixel_size=N           Pixel block size (default 15)" << endl;
        cout << "  --mask_image=path        Mask image path" << endl;
        cout << "  --stream=url             HTTP stream URL" << endl;
        cout << "\nExamples:" << endl;
        cout << "  ./privacy_protector --mode=blur --blur_size=51" << endl;
        cout << "  ./privacy_protector --mode=pixel --pixel_size=15" << endl;
        cout << "  ./privacy_protector --mode=mask --mask_image=emoji.png" << endl;
        return 0;
    }

    // 获取参数值
    string mode = parser.get<string>("mode");
    int blur_size = parser.get<int>("blur_size");
    int pixel_size = parser.get<int>("pixel_size");
    string mask_path = parser.get<string>("mask_image");
    int device = parser.get<int>("device");
    string stream_url = parser.get<string>("stream");

    // 检查参数是否解析成功
    if (!parser.check()) {
        parser.printErrors();
        cout << "\nUse --help for usage information." << endl;
        return -1;
    }

    // 验证模式
    if (mode != "blur" && mode != "pixel" && mode != "mask") {
        cerr << "Invalid mode: " << mode << ". Use blur, pixel, or mask." << endl;
        return -1;
    }

    // 确保 blur_size 是奇数
    if (blur_size % 2 == 0) blur_size += 1;
    if (blur_size < 3) blur_size = 3;

    // 确保 pixel_size 有效
    if (pixel_size < 2) pixel_size = 2;

    // 输出参数
    cout << "=== Privacy Protector ===" << endl;
    cout << "Mode: " << mode << endl;
    cout << "Blur size: " << blur_size << endl;
    cout << "Pixel size: " << pixel_size << endl;
    cout << "Mask path: " << mask_path << endl;
    cout << "=========================" << endl;

    // 步骤3: 加载 YuNet 模型
    string model_path = "face_detection_yunet_2023mar.onnx";
    float score_threshold = 0.6f;
    float nms_threshold = 0.3f;
    int top_k = 5000;

    Ptr<FaceDetectorYN> detector = FaceDetectorYN::create(
        model_path, "", Size(320, 320),
        score_threshold, nms_threshold, top_k
    );

    if (detector.empty()) {
        cerr << "Failed to load YuNet model!" << endl;
        return -1;
    }
    cout << "YuNet model loaded successfully." << endl;

    // 加载遮罩图片（仅 mask 模式需要）
    Mat mask_image = imread(mask_path, IMREAD_UNCHANGED);
    if (mask_image.empty()) {
        cerr << "Warning: Could not load mask image: " << mask_path << endl;
        cerr << "Mask mode will not work. Make sure the file exists." << endl;
    } else {
        cout << "Mask image loaded: " << mask_path 
             << " (" << mask_image.cols << "x" << mask_image.rows 
             << ", " << mask_image.channels() << " channels)" << endl;
    }

    // 步骤4: 打开摄像头/视频流
    VideoCapture cap;
    
    // 如果提供了 stream URL，使用它；否则使用默认
    if (stream_url.empty()) {
        stream_url = "http://10.26.171.247:5000/video";  // 你的默认流地址
    }
    
    cout << "Connecting to: " << stream_url << endl;
    cap.open(stream_url);

    if (!cap.isOpened()) {
        cerr << "Failed to open camera stream!" << endl;
        return -1;
    }
    cout << "Camera connected!" << endl;

    Mat frame;
    string window_name = "Privacy Protector";
    namedWindow(window_name, WINDOW_AUTOSIZE);

    int fail_count = 0;
    const int MAX_FAILS = 30;

    // 显示控制提示
    cout << "\nControls:" << endl;
    cout << "  ESC - Exit" << endl;
    cout << "  1   - Blur mode" << endl;
    cout << "  2   - Pixel mode" << endl;
    cout << "  3   - Mask mode" << endl;
    cout << "  [/] - Decrease / Increase parameter" << endl;
    cout << "  U   - Upload new mask image" << endl;

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            fail_count++;
            if (fail_count >= MAX_FAILS) {
                cerr << "Reconnecting..." << endl;
                cap.release();
                cap.open(stream_url);
                fail_count = 0;
                if (!cap.isOpened()) break;
            }
            waitKey(100);
            continue;
        }
        fail_count = 0;

        // 检测人脸
        detector->setInputSize(frame.size());
        Mat faces;
        detector->detect(frame, faces);

        // 应用隐私保护
        for (int i = 0; i < faces.rows; ++i) {
            Rect face_roi(
                static_cast<int>(faces.at<float>(i, 0)),
                static_cast<int>(faces.at<float>(i, 1)),
                static_cast<int>(faces.at<float>(i, 2)),
                static_cast<int>(faces.at<float>(i, 3))
            );
            face_roi &= Rect(0, 0, frame.cols, frame.rows);
            
            if (face_roi.width <= 0 || face_roi.height <= 0) continue;

            Mat roi = frame(face_roi);

            if (mode == "blur") {
                GaussianBlur(roi, roi, Size(blur_size, blur_size), 0);
            }
            else if (mode == "pixel") {
                Mat small;
                resize(roi, small, Size(), 1.0/pixel_size, 1.0/pixel_size, INTER_LINEAR);
                resize(small, roi, roi.size(), 0, 0, INTER_NEAREST);
            }
            else if (mode == "mask") {
                if (!mask_image.empty()) {
                    Mat resized_mask;
                    resize(mask_image, resized_mask, face_roi.size());
                    // Alpha 混合
                    if (resized_mask.channels() == 4) {
                        vector<Mat> channels;
                        split(resized_mask, channels);
                        Mat alpha = channels[3];
                        Mat mask_bgr;
                        merge(vector<Mat>{channels[0], channels[1], channels[2]}, mask_bgr);

                        // ===== 优化后的 Alpha 混合（矢量化） =====
                        Mat alpha_f, inv_alpha_f;
                        alpha.convertTo(alpha_f, CV_32F, 1.0/255.0);
                        inv_alpha_f = 1.0 - alpha_f;

                        // 转为 float 做混合
                        Mat roi_f, mask_f;
                        roi.convertTo(roi_f, CV_32FC3);
                        mask_bgr.convertTo(mask_f, CV_32FC3);

                        // 广播 alpha 到 3 通道
                        Mat alpha_3ch;
                        cvtColor(alpha_f, alpha_3ch, COLOR_GRAY2BGR);
                        Mat inv_alpha_3ch;
                        cvtColor(inv_alpha_f, inv_alpha_3ch, COLOR_GRAY2BGR);

                        // 矢量化混合
                        Mat blended = inv_alpha_3ch.mul(roi_f) + alpha_3ch.mul(mask_f);
                        blended.convertTo(roi, CV_8UC3);
                    }
                    else {
                        // 无透明通道，直接覆盖
                        if (resized_mask.channels() == 3) {
                            resized_mask.copyTo(roi);
                        }
                    }
                }
            }
        }

        for (int i = 0; i < faces.rows; ++i) {
            Rect box(
                static_cast<int>(faces.at<float>(i, 0)),
                static_cast<int>(faces.at<float>(i, 1)),
                static_cast<int>(faces.at<float>(i, 2)),
                static_cast<int>(faces.at<float>(i, 3))
            );
            box &= Rect(0, 0, frame.cols, frame.rows);
            rectangle(frame, box, Scalar(0, 255, 0), 1);  // 细绿框，仅提示
        }

        // 显示状态
        string status = "Mode: " + mode;
        if (mode == "blur") status += " (size=" + to_string(blur_size) + ")";
        if (mode == "pixel") status += " (size=" + to_string(pixel_size) + ")";
        status += " | Faces: " + to_string(faces.rows);
        
        putText(frame, status, Point(10, 30), 
                    FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

        imshow(window_name, frame);

        // 按键处理
        int key = waitKey(1) & 0xFF;
        if (key == 27) break;  // ESC
        else if (key == '1') mode = "blur";
        else if (key == '2') mode = "pixel";
        else if (key == '3') mode = "mask";
        else if (key == ']' || key == '}') {
            if (mode == "blur") {
                blur_size += 2;
                cout << "Blur: " << blur_size << endl;
            }
            if (mode == "pixel") {
                pixel_size += 2;
                cout << "Pixel: " << pixel_size << endl;
            }
        }
        else if (key == '[' || key == '{') {
            if (mode == "blur") {
                blur_size = max(3, blur_size - 2);
                cout << "Blur: " << blur_size << endl;
            }
            if (mode == "pixel") {
                pixel_size = max(2, pixel_size - 2);
                cout << "Pixel: " << pixel_size << endl;
            }
        }
        else if (key == 'u' || key == 'U') {
            cout << "\nPausing stream. Enter new mask path (or press Enter to cancel): " << flush;
            cap.release();  // 暂停视频流
    
            string new_mask_path;
            getline(cin, new_mask_path);
    
            if (!new_mask_path.empty()) {
                Mat new_mask = imread(new_mask_path, IMREAD_UNCHANGED);
                if (!new_mask.empty()) {
                    mask_image = new_mask;
                    mask_path = new_mask_path;
                    cout << "New mask loaded: " << new_mask_path << endl;
                } 
                else {
                    cerr << "Failed to load: " << new_mask_path << endl;
                }
            }
    
            cout << "Reconnecting to stream..." << endl;
            cap.open(stream_url);  // 重新连接
        }
    }

    cap.release();
    destroyAllWindows();
    return 0;
}