#include "ONNX.h"
#include <yaml-cpp/yaml.h>

ONNX::YOLO::YOLO(const std::string& model_path, const std::string& yaml_path):_OrtMemoryInfo(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPUOutput)) {
    _className = ResolveYAML(yaml_path);
    ReadModel(model_path);
    _colorSet = GenerateColor();
}

std::vector<std::string> ONNX::YOLO::ResolveYAML(const std::string& yamlPath) {
    YAML::Node config = YAML::LoadFile(yamlPath);
    std::vector<std::string> classNames;

    if (!config["names"].IsMap()) {
        throw std::runtime_error("YAML file does not contain a map of class names");
    }

    for (const auto& item : config["names"]) {
        if (!item.second.IsScalar()) {
            throw std::runtime_error("YAML node is not a scalar");
        }
        classNames.push_back(item.second.as<std::string>());
    }

    return classNames;
}

std::vector<cv::Scalar> ONNX::YOLO::GenerateColor() {
    std::vector<cv::Scalar> color;
    srand((time(nullptr)));
for (size_t i = 0; i < this->_className.size(); i++) {
        int b = rand() % 256; // 随机数为0～255
        int g = rand() % 256;
        int r = rand() % 256;
        color.push_back(cv::Scalar(b, g, r));
    }
    return color;
}

bool ONNX::YOLO::ReadModel(const std::string &modelPath){
    if (_batchSize < 1) _batchSize =1;
    try {
        std::vector<std::string> available_providers = Ort::GetAvailableProviders();
        //设置内部线程
        _OrtSessionOptions.SetIntraOpNumThreads(8);
        // 开启图像优化
        _OrtSessionOptions.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);
        _OrtSession = new Ort::Session(_OrtEnv, modelPath.c_str(), _OrtSessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;
        // 设置内存分配器，并设置为使用CPU内存
        _inputNodesNum = _OrtSession->GetInputCount();
        _inputName = std::move(_OrtSession->GetInputNameAllocated(0, allocator));
        _inputNodeNames.push_back(_inputName.get());

        Ort::TypeInfo inputTypeInfo = _OrtSession->GetInputTypeInfo(0);
        auto input_tensor_info = inputTypeInfo.GetTensorTypeAndShapeInfo();
        _inputNodeDataType = input_tensor_info.GetElementType();
        _inputTensorShape = input_tensor_info.GetShape();

        if (_inputTensorShape[0] == -1) {
            _isDynamicShape = true;
            _inputTensorShape[0] = _batchSize;
        }
        if (_inputTensorShape[2] == -1 || _inputTensorShape[3] == -1) {
            _isDynamicShape = true;
            _inputTensorShape[2] = _netHeight;
            _inputTensorShape[3] = _netWidth;
        }
        //init output
        _outputNodesNum = _OrtSession->GetOutputCount();

        _output_name0 = std::move(_OrtSession->GetOutputNameAllocated(0, allocator));
        _outputNodeNames.push_back(_output_name0.get());
        Ort::TypeInfo type_info_output0(nullptr);
        type_info_output0 = _OrtSession->GetOutputTypeInfo(0);  //output0
        auto tensor_info_output0 = type_info_output0.GetTensorTypeAndShapeInfo();
        _outputNodeDataType = tensor_info_output0.GetElementType();
        _outputTensorShape = tensor_info_output0.GetShape();
    }
    catch (const std::exception&) {
        return false;
    }
    return true;
}

int ONNX::YOLO::Preprocessing(const std::vector<cv::Mat> &SrcImages, std::vector<cv::Mat> &OutSrcImages, std::vector<cv::Vec4d> &params) const{
    OutSrcImages.clear();
    auto input_size = cv::Size(_netWidth, _netHeight);
    // 信封处理
    for (size_t i=0; i<SrcImages.size(); ++i){
        cv::Mat temp_img = SrcImages[i];
        cv::Vec4d temp_param = {1,1,0,0};
        if (temp_img.size() != input_size) {
            cv::Mat borderImg;
            LetterBox(temp_img, borderImg, temp_param, input_size, false, false, true, 32);
            OutSrcImages.push_back(borderImg);
            params.push_back(temp_param);
        }
        else {
            OutSrcImages.push_back(temp_img);
            params.push_back(temp_param);
        }
    }
    if (int lack_num = _batchSize - SrcImages.size(); lack_num > 0){
        cv::Mat temp_img = cv::Mat::zeros(input_size, CV_8UC3);
        cv::Vec4d temp_param = {1,1,0,0};
        OutSrcImages.push_back(temp_img);
        params.push_back(temp_param);
    }
    return 0;
}

bool ONNX::YOLO::OnnxBatchDetect(std::vector<cv::Mat> &SrcImages, std::vector<std::vector<OutputDet> > &output) {
    std::vector<cv::Vec4d> params;
    std::vector<cv::Mat> input_images;
    cv::Size input_size(_netWidth, _netHeight);

    Preprocessing(SrcImages, input_images, params);//preprocessing (信封处理)
    // [0~255] --> [0~1]; BGR2RGB
    cv::Mat blob = cv::dnn::blobFromImages(input_images, 1 / 255.0, input_size, cv::Scalar(0,0,0), true, false);
    // 前向传播得到推理结果
    int64_t input_tensor_length = VectorProduct(_inputTensorShape);// ?
    std::vector<Ort::Value> input_tensors;
    std::vector<Ort::Value> output_tensors;
    input_tensors.push_back(Ort::Value::CreateTensor<float>(_OrtMemoryInfo, reinterpret_cast<float*>(blob.data),
                                                            input_tensor_length, _inputTensorShape.data(),
                                                            _inputTensorShape.size()));
    output_tensors = _OrtSession->Run(Ort::RunOptions{ nullptr },
        _inputNodeNames.data(),
        input_tensors.data(),
        _inputNodeNames.size(),
        _outputNodeNames.data(),
        _outputNodeNames.size()
    );
    //post-process
    int net_width = _className.size() + 4;
    auto* all_data = output_tensors[0].GetTensorMutableData<float>(); // 第一张图片的输出
    _outputTensorShape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape(); // 一张图片输出的维度信息 [1, 84, 8400]
    int64_t one_output_length = VectorProduct(_outputTensorShape) / _outputTensorShape[0]; // 一张图片输出所占内存长度 8400*84
    for (int img_index = 0; img_index < SrcImages.size(); ++img_index){
        cv::Mat output0 = cv::Mat(cv::Size(static_cast<int>(_outputTensorShape[2]), static_cast<int>(_outputTensorShape[1])), CV_32F, all_data).t(); // [1, 84 ,8400] -> [1, 8400, 84]
        all_data += one_output_length; //指针指向下一个图片的地址
        auto* pdata = reinterpret_cast<float*>(output0.data); // [x,y,w,h,class1,class2.....class80]
        int rows = output0.rows; // 预测框的数量 8400
        // 一张图片的预测框
        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        for (int r=0; r<rows; ++r) {
            cv::Mat scores(1, _className.size(), CV_32F, pdata + 4); // 80个类别的概率
            // 得到最大类别概率、类别索引
            cv::Point classIdPoint;
            double max_class_scores; // 最大类别概率
            minMaxLoc(scores, 0, &max_class_scores, 0, &classIdPoint);
            max_class_scores = static_cast<float>(max_class_scores);
            // 预测框坐标映射到原图上
            if (max_class_scores >= _classThreshold){
                // rect [x,y,w,h]
                float x = (pdata[0] - params[img_index][2]) / params[img_index][0]; //x
                float y = (pdata[1] - params[img_index][3]) / params[img_index][1]; //y
                float w = pdata[2] / params[img_index][0]; //w
                float h = pdata[3] / params[img_index][1]; //h
                int left = MAX(int(x - 0.5 *w +0.5), 0);
                int top = MAX(int(y - 0.5*h + 0.5), 0);
                class_ids.push_back(classIdPoint.x);
                confidences.push_back(max_class_scores);
                boxes.push_back(cv::Rect(left, top, static_cast<int>(w + 0.5), static_cast<int>(h + 0.5)));
            }
            pdata += net_width; //下一个预测框
        }
        // 对一张图的预测框执行非极大值抑制
        std::vector<int> nms_result;
        cv::dnn::NMSBoxes(boxes, confidences, _classThreshold, _nmsThreshold, nms_result);
        // 对一张图片：依据非极大值抑制处理得到的索引，得到类别id、confidence、box，并置于结构体OutputDet的容器中
        std::vector<OutputDet> temp_output;
        for (size_t i=0; i<nms_result.size(); ++i){
            int idx = nms_result[i];
            OutputDet result;
            result.id = class_ids[idx];
            result.confidence = confidences[idx];
            result.box = boxes[idx];
            temp_output.push_back(result);
        }
        output.push_back(temp_output); // 多张图片的输出；添加一张图片的输出置于此容器中
    }
    if (!output.empty())
        return true;
    return false;
}
bool ONNX::YOLO::OnnxDetect(const cv::Mat &srcImg, std::vector<OutputDet> &output){
    std::vector<cv::Mat> input_data = {srcImg};
    if(std::vector<std::vector<OutputDet>> temp_output; OnnxBatchDetect(input_data, temp_output)){
        output = temp_output[0];
        return true;
    }
    return false;
}

void ONNX::YOLO::LetterBox(const cv::Mat& image, cv::Mat& outImage, cv::Vec4d& params, const cv::Size& newShape, bool autoShape, bool scaleFill, bool scaleUp, int stride, const cv::Scalar& color) {
    // 取较小的缩放比例
    cv::Size shape = image.size();
    float r = std::min(static_cast<float>(newShape.height) / static_cast<float>(shape.height), static_cast<float>(newShape.width) / static_cast<float>(shape.width));
    if (!scaleUp)
        r = std::min(r, 1.0f);
    // 依据前面的缩放比例后，原图的尺寸
    float ratio[2]{r,r};
    int new_un_pad[2] = { static_cast<int>(std::round(static_cast<float>(shape.width) * r)), static_cast<int>(std::round(static_cast<float>(shape.height) * r))};
    // 计算距离目标尺寸的padding像素数
    auto dw = static_cast<float>(newShape.width - new_un_pad[0]);
    auto dh = static_cast<float>(newShape.height - new_un_pad[1]);
    if (autoShape) {
        dw = static_cast<float>(static_cast<int>(dw) % stride);
        dh = static_cast<float>(static_cast<int>(dh) % stride);
    }
    else if (scaleFill) {
        dw = 0.0f;
        dh = 0.0f;
        new_un_pad[0] = newShape.width;
        new_un_pad[1] = newShape.height;
        ratio[0] = static_cast<float>(newShape.width) / static_cast<float>(shape.width);
        ratio[1] = static_cast<float>(newShape.height) / static_cast<float>(shape.height);
    }
    dw /= 2.0f;
    dh /= 2.0f;
    // 等比例缩放
    if (shape.width != new_un_pad[0] && shape.height != new_un_pad[1]) {
        cv::resize(image, outImage, cv::Size(new_un_pad[0], new_un_pad[1]));
    }
    else {
        outImage = image.clone();
    }
    // 图像四周padding填充，至此原图与目标尺寸一致
    int top = static_cast<int>(std::round(dh - 0.1f));
    int bottom = static_cast<int>(std::round(dh + 0.1f));
    int left = static_cast<int>(std::round(dw - 0.1f));
    int right = static_cast<int>(std::round(dw + 0.1f));
    params[0] = ratio[0]; // width的缩放比例
    params[1] = ratio[1]; // height的缩放比例
    params[2] = left; // 水平方向两边的padding像素数
    params[3] = top; //垂直方向两边的padding像素数
    cv::copyMakeBorder(outImage, outImage, top, bottom, left, right, cv::BORDER_CONSTANT, color);
}

void ONNX::YOLO::DrawPred(cv::Mat& img, const std::vector<OutputDet>& result, const std::vector<std::string>& classNames, const std::vector<cv::Scalar>& color) {
    for (size_t i=0; i<result.size(); i++) {
        int top;
        int left = result[i].box.x;
        top = result[i].box.y;
        // 框出目标
        rectangle(img, result[i].box,color[result[i].id], 3, cv::LINE_AA);
        // 在目标框左上角标识目标类别以及概率
        std::string label = classNames[result[i].id] + ":" + std::to_string(result[i].confidence);
        int baseLine;
        cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.8, 1, &baseLine);
        top = std::max(top, labelSize.height);
        putText(img, label, cv::Point(left, top-10), cv::FONT_HERSHEY_SIMPLEX, 1, color[result[i].id], 2);
        std::cout << "Class ID: " << result[i].id << ", Name: " << classNames[result[i].id] << ", Confidence: " << result[i].confidence << std::endl;
    }
}

cv::Mat ONNX::YOLO::yoloDetect(cv::Mat& srcImg) {
    std::vector<OutputDet> output;
    if (OnnxDetect(srcImg, output)) {
        DrawPred(srcImg, output, _className, _colorSet);
    }
    return srcImg;
}