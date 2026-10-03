// ============================================================================
// main.cpp —— 演示 Raytracer 的用法：渲染一个「球 + 背景色」的场景并写盘
//
// 场景由 raytracer.h 里的 Raytracer 负责，位图读写由 bitmap.h 里的 Bitmap 负责，
// 本文件只做三件事：摆好场景参数 → 渲染 → 保存。
//
// 编译：cl /nologo /std:c++17 /W4 /utf-8 /EHsc /O2 main.cpp bitmap.cpp raytracer.cpp
// 运行：main.exe（或改名后的可执行文件），会在当前目录生成 sphere.bmp
// ============================================================================

#include "bitmap.h"
#include "raytracer.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

// ============================================================================
// 场景参数（想换效果直接改这几个常量即可）
// ============================================================================

// 位图分辨率，也就是投影平面上「一个像素」的疏密
const int kWidth  = 640;
const int kHeight = 480;

// 摄像机到投影平面的直线距离 D
const double kDistance = 500.0;

// 背景色：射线没有打到任何球时用的颜色 {红, 绿, 蓝}
const uint8_t kBackground[3] = {0, 0, 0};

// 球体：半径 R、颜色 color、位置（球心）
const double  kSphereRadius = 150.0;
const uint8_t kSphereColor[3] = {255, 80, 0};
const Vector  kSphereCenter(0.0, 0.0, 800.0);

// 输出路径
const std::string kOutputPath = "sphere.bmp";

/**
 * 作用：程序入口。按上面的参数搭好场景、渲染、保存，并打印一行摘要。
 * 参数：无（不使用命令行参数；如需从命令行读分辨率/距离，可改成
 *       int main(int argc, char* argv[]) 再用 std::stod 解析）
 * 返回：全部成功返回 0；参数非法、保存失败或发生异常返回 1
 */
int main()
{
    try {
        // 步骤 1：构造光线投射器 —— 摄像机固定在原点，投影平面在 z = D 处。
        Raytracer tracer(kWidth, kHeight, kDistance, kBackground);

        // 步骤 2：往场景里放一个球。
        tracer.addSphere(Sphere(kSphereCenter, kSphereRadius, kSphereColor));

        // 步骤 3：渲染。每个像素对应一条从原点出发、穿过该像素的射线，
        //         与球求交时判别式 delta >= 0 就涂球色，否则涂背景色。
        const Bitmap image = tracer.render();

        // 步骤 4：写盘。save 用返回值报告失败，不抛异常。
        if (!image.save(kOutputPath)) {
            std::cerr << "保存失败：" << kOutputPath << "\n";
            return 1;
        }

        // 步骤 5：打印摘要。命中区域是球在投影平面上的轮廓，
        //         边界 q = R 对应 x^2 + y^2 = R^2 * D^2 / (Cz^2 - R^2)，
        //         所以命中圆的半径是 R*D / sqrt(Cz^2 - R^2)。
        const double r2 = kSphereCenter.z() * kSphereCenter.z()
                        - kSphereRadius * kSphereRadius;
        const double projectedRadius =
            (r2 > 0.0) ? kSphereRadius * kDistance / std::sqrt(r2) : 0.0;

        std::cout << "已生成位图：" << kOutputPath
                  << "  " << image.width() << " x " << image.height() << "\n"
                  << "摄像机 O(0,0,0)，投影平面 z = " << tracer.distance() << "\n"
                  << "场景球数：" << tracer.sphereCount()
                  << "  R = " << kSphereRadius
                  << "  球心 (" << kSphereCenter.x() << ", " << kSphereCenter.y()
                  << ", " << kSphereCenter.z() << ")"
                  << "  颜色 RGB(" << static_cast<int>(kSphereColor[0]) << ", "
                                   << static_cast<int>(kSphereColor[1]) << ", "
                                   << static_cast<int>(kSphereColor[2]) << ")\n"
                  << "背景色 RGB(" << static_cast<int>(kBackground[0]) << ", "
                                   << static_cast<int>(kBackground[1]) << ", "
                                   << static_cast<int>(kBackground[2]) << ")\n"
                  << "命中圆半径约 " << projectedRadius << " 像素\n";
    }
    catch (const std::exception& e) {
        // Raytracer / Sphere 构造可能因参数非法抛 std::invalid_argument，
        // Bitmap 构造可能因宽高非法抛 std::invalid_argument，这里统一兜住。
        std::cerr << "错误：" << e.what() << "\n";
        return 1;
    }

    return 0;
}
