#pragma once

#include <QLibrary>
#include <QString>

#include <string>

#include "markInterface.h"


class MarkInterfaceLoader {
public:
    explicit MarkInterfaceLoader(const QString &dllPath = QString()) {
        if (!dllPath.isEmpty()) load(dllPath);
    }

public:
    bool GenerateMarkTemplate(const char *image_path,
                              const char *save_model_dir,
                              const char *model_name,
                              int binarize_thresh,
                              const MarkRect *roi) {
        if (!generateMarkTemplate) return false;
        return generateMarkTemplate(image_path, save_model_dir, model_name, binarize_thresh, roi);
    }

    bool InitMarkMatcher(const char *model_dir, const char *model_name) {
        if (!initMarkMatcher) return false;
        initialized_ = initMarkMatcher(model_dir, model_name);
        return initialized_;
    }

    bool RunMarkMatchingSingle(const char *search_image_path,
                               const char *output_path,
                               double *x, double *y, double *r,
                               float *similarity,
                               double *time_ms,
                               int *binarize_thresh,
                               const MarkRect *roi) {
        if (!runMarkMatchingSingle) return false;
        if (!initialized_) return false;
        return runMarkMatchingSingle(search_image_path, output_path,
                                     x, y, r, similarity, time_ms,
                                     binarize_thresh, roi);
    }

    typedef bool (*GenerateMarkTemplateFunc)(
        const char *image_path,
        const char *save_model_dir,
        const char *model_name,
        int binarize_thresh,
        const MarkRect *roi
    );

    typedef bool (*InitMarkMatcherFunc)(
        const char *model_dir,
        const char *model_name
    );

    typedef bool (*RunMarkMatchingSingleFunc)(
        const char *search_image_path,
        const char *output_path,
        double *x,
        double *y,
        double *r,
        float *similarity,
        double *time_ms,
        int *binarize_thresh,
        const MarkRect *roi
    );

    // dllPath: "algorithm" / "algorithm.dll" / "绝对路径" / "相对路径"
    bool load(const QString &dllPath) {
        lib.setFileName(dllPath);
        if (!lib.load()) return false;

        generateMarkTemplate = reinterpret_cast<GenerateMarkTemplateFunc>(
            lib.resolve("GenerateMarkTemplate"));
        initMarkMatcher = reinterpret_cast<InitMarkMatcherFunc>(
            lib.resolve("InitMarkMatcher"));
        runMarkMatchingSingle = reinterpret_cast<RunMarkMatchingSingleFunc>(
            lib.resolve("RunMarkMatchingSingle"));

        return generateMarkTemplate && initMarkMatcher && runMarkMatchingSingle;
    }

    bool isLoaded() const { return lib.isLoaded(); }

    QString errorString() const { return lib.errorString(); }

private:
    QLibrary lib;

    bool initialized_ = false;

    GenerateMarkTemplateFunc generateMarkTemplate = nullptr;
    InitMarkMatcherFunc initMarkMatcher = nullptr;
    RunMarkMatchingSingleFunc runMarkMatchingSingle = nullptr;
};


// Example:
void markInterfaceLoaderTest() {
    MarkInterfaceLoader mark("algorithm");
    if (!mark.isLoaded()) {
        std::cout << "DLL load failed: " << mark.errorString().toStdString() << std::endl;
        return;
    }

    // Parameter
    std::string mark_image_path = R"(D:\xxx.bmp)";
    std::string save_model_dir = R"(D:\xxx)";
    std::string model_name = "xxx";
    int binThresh = 160;
    const MarkRect roi{100, 100, 100, 100};  // or nullptr

    std::string search_image_path = R"(D:\xxx.bmp)";
    std::string output_path = R"(D:\xxx.jpg)";

    // 1) 生成模板
    bool ok = mark.GenerateMarkTemplate(mark_image_path.c_str(), save_model_dir.c_str(), model_name.c_str(), binThresh, &roi);
    std::cout << "GenerateMarkTemplate result: " << ok << std::endl;

    // 2) 匹配前必须初始化（只需一次，后续复用）
    ok = mark.InitMarkMatcher(save_model_dir.c_str(), model_name.c_str()); // 必须，否则匹配失败
    if (!ok) {
        std::cout << "InitMarkMatcher failed!" << std::endl;
        return;
    }

    // 3) 单图匹配
    double x = 0, y = 0, r = 0;
    float similarity = 0.0f;
    double time_ms = 0;
    ok = mark.RunMarkMatchingSingle(search_image_path.c_str(), output_path.c_str(),
                                    &x, &y, &r, &similarity, &time_ms, &binThresh, nullptr);
    std::cout << "RunMarkMatchingSingle result: " << ok
              << " x: " << x << " y: " << y << " r: " << r
              << " similarity: " << similarity << " time: " << time_ms << "ms" << std::endl;

    return;
}

