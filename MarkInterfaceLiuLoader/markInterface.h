#pragma once

#include <opencv2/core.hpp>

#ifdef MARK_EXPORTS
#define MARK_API __declspec(dllexport)
#else
#define MARK_API __declspec(dllimport)
#endif


// 自定义矩形结构体（替代cv::Rect）
typedef struct {
    int x;          // 左上角X坐标
    int y;          // 左上角Y坐标
    int width;      // 宽度
    int height;     // 高度
} MarkRect;


extern "C" {

// ① 创建模板（从图像生成模板并保存）
MARK_API bool GenerateMarkTemplate(
        const char *image_path,
        const char *save_model_dir,
        const char *model_name,
        int binarize_thresh,
        const MarkRect *roi  // 自定义ROI结构体，传NULL表示全图
);

MARK_API bool InitMarkMatcher(
        const char *model_dir,
        const char *model_name
);

// ② 匹配阶段：输入模型路径和待匹配图，输出绘制图
MARK_API bool RunMarkMatchingSingle(
        const char *search_image_path,
        const char *output_path,
        double *x,
        double *y,
        double *r,
        float *similarity,
        double *time_ms,
        int *binarize_thresh,  // 传入二值化阈值
        const MarkRect *roi    // 自定义ROI结构体，传NULL表示全图
);

MARK_API bool RunMarkMatchingWithModel(
        const char *search_image_path,
        const char *model_dir,
        const char *model_name,
        const char *output_path
);

}
