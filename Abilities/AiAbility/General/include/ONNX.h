#ifndef ONNX_H
#define ONNX_H

#include <memory>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <numeric>

#define CLASS_THERESHOLD 0.2
#define NET_WIDTH 320
#define NET_HEIGHT NET_WIDTH

namespace ONNX {
    struct OutputDet{
        int id;
        float confidence;
        cv::Rect box;
    };

    class YOLO{
    public:
        YOLO(const std::string& model_path, const std::string& yaml_path);
        ~YOLO() = default;

        cv::Mat yoloDetect(cv::Mat& srcImg);
        bool ReadModel(const std::string& modelPath);
        bool OnnxDetect(const cv::Mat& srcImg, std::vector<OutputDet>& output);
        bool OnnxBatchDetect(std::vector<cv::Mat>& srcImgs, std::vector<std::vector<OutputDet>>& output);
        static void DrawPred(cv::Mat& img, const std::vector<OutputDet>& result, const std::vector<std::string>& classNames, const std::vector<cv::Scalar>& color);
        static void LetterBox(const cv::Mat& image, cv::Mat& outImage, cv::Vec4d& params,
                                const cv::Size& newShape = cv::Size(640, 640), bool autoShape = false,
                                bool scaleFill=false, bool scaleUp=true, int stride= 32,const cv::Scalar& color = cv::Scalar(114,114,114));
        std::vector<std::string> _className;

    private:
        template <typename Templeate>   // 创建一个模板函数，用于计算向量的乘积
        Templeate VectorProduct(const std::vector<Templeate>& v) {
            return std::accumulate(v.begin(), v.end(), 1, std::multiplies<Templeate>());
        }
        std::vector<cv::Scalar> GenerateColor();
        int Preprocessing(const std::vector<cv::Mat>& SrcImgs, std::vector<cv::Mat>& OutSrcImgs, std::vector<cv::Vec4d>& params) const;
        static std::vector<std::string> ResolveYAML(const std::string& yamlPath);

        const int _netWidth = NET_WIDTH;   //ONNX网络输入宽度
        const int _netHeight = NET_HEIGHT;  //ONNX网络输入高度

        int _batchSize = 1; //if multi-batch,set this
        bool _isDynamicShape = true;   //onnx 支持动态shape
        float _classThreshold = CLASS_THERESHOLD;   // 置信度
        float _nmsThreshold= 0.45;
        float _maskThreshold = 0.5;

        Ort::Env _OrtEnv = Ort::Env(ORT_LOGGING_LEVEL_ERROR, "Yolov11n");
        Ort::SessionOptions _OrtSessionOptions = Ort::SessionOptions();
        Ort::Session* _OrtSession = nullptr;
        Ort::MemoryInfo _OrtMemoryInfo;
        std::shared_ptr<char> _inputName, _output_name0;
        std::vector<char*> _inputNodeNames; //输入节点名
        std::vector<char*> _outputNodeNames; // 输出节点名
        size_t _inputNodesNum = 0;        // 输入节点数
        size_t _outputNodesNum = 0;      // 输出节点数
        ONNXTensorElementDataType _inputNodeDataType;  //数据类型
        ONNXTensorElementDataType _outputNodeDataType;
        std::vector<int64_t> _inputTensorShape;  // 输入张量形状
        std::vector<int64_t> _outputTensorShape;
        std::vector<cv::Scalar> _colorSet;
    };
}

#endif //ONNX_H