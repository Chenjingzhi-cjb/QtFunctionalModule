#ifndef CAMERA_CONTROLLER_HPP
#define CAMERA_CONTROLLER_HPP

#include <QObject>
#include <QMutex>
#include <QMutexLocker>

#include <iostream>
#include <exception>
#include <mutex>
#include <thread>

#include "GalaxyIncludes.h"
#include "opencv2/opencv.hpp"


class CameraController : public QObject {
    Q_OBJECT

private:
    CameraController()
            : m_bIsOpen(false),
              m_bIsSnap(false),
              m_image_height(0),
              m_image_width(0),
              m_exposure_time_ms(10000),
              m_exposure_gain_dB(0) {
        cameraInit();
    }

    ~CameraController() = default;

public:
    static CameraController &getInstance() {
        static CameraController instance;  // 局部静态变量，注意生命周期

        return instance;
    }

    void exitCamera() {
        cameraClose();
    }

public:
    int getImageWidth() {
        return m_image_width;
    }

    int getImageHeight() {
        return m_image_height;
    }

    cv::Mat getImage() {
        QMutexLocker locker(&m_image_mutex);

        cv::Mat image(m_image_height, m_image_width, CV_8UC1, m_pBuffer);

        return image;
    }

    QMutex *getImageMutex() {
        return &m_image_mutex;
    }

    void *getBufferPtr() {
        return m_pBuffer;
    }

    int getExposureTimeMs() {
        return m_exposure_time_ms;
    }

    void setExposureTimeMs(int exposure_time_ms) {
        m_exposure_time_ms = exposure_time_ms;
        m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->SetValue(m_exposure_time_ms);
    }

    int getExposureGainDB() {
        return m_exposure_gain_dB;
    }

    void setExposureGainDB(int exposure_gain_dB) {
        m_exposure_gain_dB = exposure_gain_dB;
        m_objFeatureControlPtr->GetFloatFeature("Gain")->SetValue(m_exposure_gain_dB);
    }

signals:
    void signalUpdateImage(void *, int, int);

private:
    // 用户继承采集事件处理类
    class CSampleCaptureEventHandler : public ICaptureEventHandler {
    public:
        void DoOnImageCaptured(CImageDataPointer &objImageDataPointer, void *pUserParam) {
            CameraController *camera = (CameraController *)pUserParam;
            QMutexLocker locker(camera->getImageMutex());

            try {
                void *image_data = camera->getBufferPtr();
                int height = objImageDataPointer->GetHeight();
                int width = objImageDataPointer->GetWidth();

                camera->setImageHeight(height);
                camera->setImageWidth(width);
                std::memcpy(image_data, objImageDataPointer->GetBuffer(), height * width);

                emit camera->signalUpdateImage(image_data, height, width);
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

        // 设置 曝光时间
        m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->SetValue(m_exposure_time_ms);

        // 设置 曝光增益
        m_objFeatureControlPtr->GetFloatFeature("Gain")->SetValue(m_exposure_gain_dB);
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
        m_bIsSnap = false;
    }

    void cameraInitTask() {
        // 初始化库
        IGXFactory::GetInstance().Init();

        m_pCaptureEventHandler = new CSampleCaptureEventHandler();

        // 打开设备
        openDevice();

        // 开始采集
        startSnap();

        // 图像数据内存空间初始化
        m_pBuffer = malloc(m_objFeatureControlPtr->GetIntFeature("WidthMax")->GetValue() * m_objFeatureControlPtr->GetIntFeature("HeightMax")->GetValue());
        if (NULL == m_pBuffer) {
            std::cout << "failed to malloc" << std::endl;
            return;
        }
    }

    void cameraInit() {
        std::thread video_camera_Thread(&CameraController::cameraInitTask, this);
        video_camera_Thread.detach();
    }

    void cameraClose() {
        // 停止采集
        stopSnap();

        // 关闭设备
        closeDevice();

        delete m_pCaptureEventHandler;
        delete m_pBuffer;
    }

private:
    CGXDevicePointer            m_objDevicePtr;                // 设备句柄
    CGXFeatureControlPointer    m_objFeatureControlPtr;        // 设备控制器对象
    CGXStreamPointer            m_objStreamPtr;                // 流对象
    CGXFeatureControlPointer    m_objStreamFeatureControlPtr;  // 流层控制器对象
    CSampleCaptureEventHandler *m_pCaptureEventHandler;        // 采集回调对象

    bool m_bIsOpen;
    bool m_bIsSnap;

    QMutex m_image_mutex;
    CImageDataPointer m_image_data_pointer;
    void *m_pBuffer = nullptr;
    int m_image_height;
    int m_image_width;

    int m_exposure_time_ms;
    int m_exposure_gain_dB;
};


#endif // CAMERA_CONTROLLER_HPP
