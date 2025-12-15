#ifndef CAMERA_CONTROLLER_HPP
#define CAMERA_CONTROLLER_HPP

#include <QObject>
#include <QImage>
#include <QTimer>

#include <iostream>
#include <exception>
#include <mutex>

#include "GalaxyIncludes.h"
#include "opencv2/opencv.hpp"


class CameraController : public QObject {
    Q_OBJECT

private:
    CameraController()
            : m_pCaptureEventHandler(nullptr),
              m_bIsOpen(false),
              m_bIsSnap(false),
              m_image_height(0),
              m_image_width(0),
              m_buffer_size(0) {
        cameraInit();
    }

    ~CameraController() {
        if (m_bIsOpen || m_bIsSnap) {
            closeCamera();
        }

        if (m_pCaptureEventHandler != nullptr) {
            delete m_pCaptureEventHandler;
            m_pCaptureEventHandler = nullptr;
        }
    }

public:
    static CameraController &getInstance() {
        static CameraController instance;  // 局部静态变量，注意生命周期
        return instance;
    }

    bool isCameraOpen() {
        return (m_bIsOpen && m_bIsSnap);
    }

    void openCamera() {
        std::lock_guard<std::mutex> locker(m_image_mutex);

        // 打开设备
        openDevice();

        if (!m_bIsOpen) return;

        // 开始采集
        startSnap();

        if (!m_bIsSnap) return;

        // 图像数据内存空间初始化
        m_image_height = m_objFeatureControlPtr->GetIntFeature("Height")->GetValue();
        m_image_width = m_objFeatureControlPtr->GetIntFeature("Width")->GetValue();
        m_buffer_size = m_image_height * m_image_width;
        m_pBuffer.resize(m_buffer_size);
    }

    void closeCamera() {
        // 停止采集
        stopSnap();

        // 关闭设备
        closeDevice();

        m_pBuffer.clear();
    }

    int getImageWidth() {
        return m_image_width;
    }

    int getImageHeight() {
        return m_image_height;
    }

    int getBufferSize() {
        return m_buffer_size;
    }

    cv::Mat getImage() {
        if (!m_bIsOpen || !m_bIsSnap) return cv::Mat();

        std::lock_guard<std::mutex> locker(m_image_mutex);

        return cv::Mat(m_image_height, m_image_width, CV_8UC1, m_pBuffer.data()).clone();
    }

    std::mutex &getImageMutex() {
        return m_image_mutex;
    }

    std::vector<uint8_t> &getBuffer() {
        return m_pBuffer;
    }

    double getExposureTimeUs() {
        return m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->GetValue();
    }

    void setExposureTimeUs(double exposure_time_us) {
        if (!m_bIsOpen || !m_bIsSnap) return;

        if ((exposure_time_us < 1000) || (exposure_time_us > 1000000)) return;

        m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->SetValue(exposure_time_us);
    }

    double getExposureGainDB() {
        return m_objFeatureControlPtr->GetFloatFeature("Gain")->GetValue();
    }

    void setExposureGainDB(double exposure_gain_dB) {
        if (!m_bIsOpen || !m_bIsSnap) return;

        if ((exposure_gain_dB < 0) || (exposure_gain_dB > 24)) return;

        m_objFeatureControlPtr->GetFloatFeature("Gain")->SetValue(exposure_gain_dB);
    }

    void setAutoExposureOnce(int wait_msec = 1000) {
        if (!m_bIsOpen || !m_bIsSnap) return;

        m_objFeatureControlPtr->GetEnumFeature("ExposureAuto")->SetValue("Once");

        QTimer::singleShot(wait_msec, [=]() {
            emit signalAutoExposureTimeUs(getExposureTimeUs());
        });
    }

signals:
    void signalUpdateImage(QImage);

    void signalAutoExposureTimeUs(double);

private:
    // 用户继承采集事件处理类
    class CSampleCaptureEventHandler : public ICaptureEventHandler {
    public:
        void DoOnImageCaptured(CImageDataPointer &objImageDataPointer, void *pUserParam) {
            CameraController *camera = static_cast<CameraController *>(pUserParam);
            std::lock_guard<std::mutex> locker(camera->getImageMutex());

            try {
                std::memcpy(camera->getBuffer().data(), objImageDataPointer->GetBuffer(), camera->getBufferSize());

                emit camera->signalUpdateImage(QImage(camera->getBuffer().data(),
                                                      camera->getImageWidth(),
                                                      camera->getImageHeight(),
                                                      QImage::Format_Grayscale8));
            } catch (CGalaxyException) {
                // do noting
            }
        }
    };

    void paramInit() {
        // 设置 采集模式 为 连续采集
        m_objFeatureControlPtr->GetEnumFeature("AcquisitionMode")->SetValue("Continuous");

        // 设置 触发模式 为 关
        m_objFeatureControlPtr->GetEnumFeature("TriggerMode")->SetValue("Off");

        // 设置 初始曝光时间
        m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->SetValue(10000);

        // 设置 初始曝光增益
        m_objFeatureControlPtr->GetFloatFeature("Gain")->SetValue(0);
    }

    void openDevice() {
        // TODO: Add your control notification handler code here
        bool bIsDeviceOpen = false;  // 设备开启标志
        bool bIsStreamOpen = false;  // 流开启标志

        try {
            // 枚举设备
            GxIAPICPP::gxdeviceinfo_vector vectorDeviceInfo;
            IGXFactory::GetInstance().UpdateDeviceList(1000, vectorDeviceInfo);
            if (vectorDeviceInfo.size() <= 0) {
                std::cout << "Device not found!" << std::endl;
                return;
            }

            // 打开设备
            m_objDevicePtr = IGXFactory::GetInstance().OpenDeviceBySN(vectorDeviceInfo[0].GetSN(), GX_ACCESS_EXCLUSIVE);
            m_objFeatureControlPtr = m_objDevicePtr->GetRemoteFeatureControl();
            bIsDeviceOpen = true;

            // 获取流通道个数
            uint32_t nStreamCount = m_objDevicePtr->GetStreamCount();
            if (nStreamCount <= 0) {
                std::cout << "Device stream not found!" << std::endl;
                return;
            }

            // 打开流
            m_objStreamPtr = m_objDevicePtr->OpenStream(0);
            m_objStreamFeatureControlPtr = m_objStreamPtr->GetFeatureControl();
            bIsStreamOpen = true;

            // 建议用户在打开网络相机之后，根据当前网络环境设置相机的流通道包长值，
            // 以提高网络相机的采集性能,设置方法参考以下代码。
            GX_DEVICE_CLASS_LIST objDeviceClass = m_objDevicePtr->GetDeviceInfo().GetDeviceClass();
            if (GX_DEVICE_CLASS_GEV == objDeviceClass) {
                // 判断设备是否支持流通道数据包功能
                if (true == m_objFeatureControlPtr->IsImplemented("GevSCPSPacketSize")) {
                    // 获取当前网络环境的最优包长值
                    int nPacketSize = m_objStreamPtr->GetOptimalPacketSize();
                    // 将最优包长值设置为当前设备的流通道包长值
                    m_objFeatureControlPtr->GetIntFeature("GevSCPSPacketSize")->SetValue(nPacketSize);
                }
            }

            // 初始化相机参数
            paramInit();

            m_bIsOpen = true;
        } catch (CGalaxyException) {
            std::cout << "Open device galaxy error!" << std::endl;

            if (bIsStreamOpen) {
                m_objStreamPtr->Close();
            }

            if (bIsDeviceOpen) {
                m_objDevicePtr->Close();
            }

            return;
        } catch (std::exception) {
            std::cout << "Open device std error!" << std::endl;

            if (bIsStreamOpen) {
                m_objStreamPtr->Close();
            }

            if (bIsDeviceOpen) {
                m_objDevicePtr->Close();
            }

            return;
        }
    }

    void startSnap() {
        // TODO: Add your control notification handler code here
        try {
            try {
                // 设置 Buffer 处理模式
                m_objStreamFeatureControlPtr->GetEnumFeature("StreamBufferHandlingMode")->SetValue("OldestFirst");
            } catch (...) {}

            // 注册回调函数
            m_objStreamPtr->RegisterCaptureCallback(m_pCaptureEventHandler, this);

            // 开启流层通道
            m_objStreamPtr->StartGrab();

            // 发送开采命令
            m_objFeatureControlPtr->GetCommandFeature("AcquisitionStart")->Execute();

            m_bIsSnap = true;
        } catch (CGalaxyException) {
            std::cout << "Open device galaxy error!" << std::endl;

            return;
        } catch (std::exception) {
            std::cout << "Open device std error!" << std::endl;

            return;
        }
    }

    void stopSnap() {
        // TODO: Add your control notification handler code here
        try {
            // 发送停采命令
            m_objFeatureControlPtr->GetCommandFeature("AcquisitionStop")->Execute();

            // 关闭流层通道
            m_objStreamPtr->StopGrab();

            // 注销采集回调
            m_objStreamPtr->UnregisterCaptureCallback();

            m_bIsSnap = false;
        } catch (CGalaxyException) {
            std::cout << "Open device galaxy error!" << std::endl;

            return;
        } catch (std::exception) {
            std::cout << "Open device std error!" << std::endl;

            return;
        }
    }

    void closeDevice() {
        // TODO: Add your control notification handler code here
        try {
            // 判断是否已停止采集
            if (m_bIsSnap) {
                // 发送停采命令
                m_objFeatureControlPtr->GetCommandFeature("AcquisitionStop")->Execute();

                // 关闭流层采集
                m_objStreamPtr->StopGrab();

                // 注销采集回调
                m_objStreamPtr->UnregisterCaptureCallback();
            }
        } catch (CGalaxyException) {
            // do noting
        }

        try {
            // 关闭流对象
            m_objStreamPtr->Close();
        } catch (CGalaxyException) {
            // do noting
        }

        try {
            // 关闭设备
            m_objDevicePtr->Close();
        } catch (CGalaxyException) {
            //do noting
        }

        m_bIsOpen = false;
    }

    void cameraInit() {
        try {
            // 初始化库
            IGXFactory::GetInstance().Init();

            m_pCaptureEventHandler = new CSampleCaptureEventHandler();
        } catch (...) {}
    }

private:
    CGXDevicePointer            m_objDevicePtr;                // 设备句柄
    CGXFeatureControlPointer    m_objFeatureControlPtr;        // 设备控制器对象
    CGXStreamPointer            m_objStreamPtr;                // 流对象
    CGXFeatureControlPointer    m_objStreamFeatureControlPtr;  // 流层控制器对象
    CSampleCaptureEventHandler *m_pCaptureEventHandler;        // 采集回调对象

    bool m_bIsOpen;
    bool m_bIsSnap;

    std::mutex m_image_mutex;
    CImageDataPointer m_image_data_pointer;
    std::vector<uint8_t> m_pBuffer;
    int m_image_height;
    int m_image_width;
    int m_buffer_size;
};


#endif // CAMERA_CONTROLLER_HPP
