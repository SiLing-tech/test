// ============================================================================
// main.cpp —— 演示怎样用一套 Settings 配置驱动渲染
//
// 本文件不再写任何场景数值：像素宽高、摄像机距离、背景色、球的大小/颜色/位置、
// 输出路径，全部来自 settings.h。想换效果只改 settings.cpp 里的 makeDefault()。
//
// 本文件只做五件事：读配置 → 校验 → 按配置建场景 → 渲染 → 存盘。
//
// 编译：cl /nologo /std:c++17 /W4 /utf-8 /EHsc /O2
//           main.cpp bitmap.cpp geometry.cpp object.cpp raytracer.cpp settings.cpp
// 运行：生成的 exe，会按配置里的路径生成位图
// ============================================================================

#include "bitmap.h"
#include "geometry.h"              // Vector
#include "object.h"                // Sphere：以后自己加的形状（如 Box）也从这里派生
#include "raytracer.h"             // Raytracer
#include "settings.h"              // Settings：所有可调参数

#include <cmath>
#include <exception>
#include <iostream>

/**
 * 作用：程序入口。按配置搭好场景、渲染、保存，并打印一行摘要。
 * 参数：无（不使用命令行参数；如需从命令行覆盖某些配置，可改成
 *       int main(int argc, char* argv[]) 解析后再改 Settings 的字段）
 * 返回：全部成功返回 0；配置非法、保存失败或发生异常返回 1
 */
int main()
{
    try {
        // 步骤 1：取一套配置。所有数值都在 settings.cpp 的 makeDefault() 里。
        const Settings::Settings settings = Settings::Settings::makeDefault();

        // 步骤 2：动手之前先把整套配置检查一遍。
        //         宽高、距离 D、每个球的半径在各自构造时已经校验过，
        //         validate() 补上「整体」层面的检查（比如场景是不是空的）。
        settings.validate();

        // 步骤 3：按配置构造光线投射器 —— 摄像机固定在原点，投影平面在 z = D 处。
        const Settings::ImageSettings& image = settings.image();
        Raytracer tracer(image.width(), image.height(), image.distance(),
                         image.background());

        // 步骤 4：按配置往场景里放球。
        //         这里就是「配置」与「类型」的交界处：数值从 Settings 里读出来，
        //         交给 Sphere 去构造。Settings 本身刻意不认识 Sphere。
        for (std::size_t i = 0; i < settings.sphereCount(); ++i) {
            const Settings::SphereSettings& s = settings.sphere(i);
            tracer.addSphere(Sphere(Vector(s.cx(), s.cy(), s.cz()),
                                    s.radius(),
                                    s.color()));
        }

        // 想加别的形状（长方体等），自己写一个继承 Object 的类，
        // 然后按下面这样加进去即可，渲染代码不用改：
        //     tracer.add(std::make_unique<Box>(...));

        // 步骤 5：渲染。每个像素对应一条从原点出发、穿过该像素的射线；
        //         Raytracer 会问场景里每个物体 obj->intersect(ray)，
        //         取最近命中的那个涂上它的颜色；都没命中就涂背景色。
        const Bitmap result = tracer.render();

        // 步骤 6：写盘。save 用返回值报告失败，不抛异常。
        if (!result.save(settings.outputPath())) {
            std::cerr << "保存失败：" << settings.outputPath() << "\n";
            return 1;
        }

        // 步骤 7：打印摘要，把生效的配置回报一遍，便于核对。
        std::cout << "已生成位图：" << settings.outputPath()
                  << "  " << result.width() << " x " << result.height() << "\n"
                  << "摄像机 O(0,0,0)，投影平面 z = " << image.distance() << "\n"
                  << "背景色 RGB(" << static_cast<int>(image.background()[0]) << ", "
                                   << static_cast<int>(image.background()[1]) << ", "
                                   << static_cast<int>(image.background()[2]) << ")\n"
                  << "场景物体数：" << tracer.objectCount() << "\n";

        for (std::size_t i = 0; i < settings.sphereCount(); ++i) {
            const Settings::SphereSettings& s = settings.sphere(i);
            std::cout << "  球 #" << i
                      << "  球心 (" << s.cx() << ", " << s.cy() << ", " << s.cz() << ")"
                      << "  R = " << s.radius()
                      << "  体积 = " << s.volume()
                      << "  颜色 RGB(" << static_cast<int>(s.color()[0]) << ", "
                                       << static_cast<int>(s.color()[1]) << ", "
                                       << static_cast<int>(s.color()[2]) << ")\n";

            // 命中区域是球在投影平面上的轮廓：边界 q = R 对应
            //     x^2 + y^2 = R^2 * D^2 / (Cz^2 - R^2)
            // 所以命中圆的半径是 R*D / sqrt(Cz^2 - R^2)。只在球位于平面前方时有意义。
            const double r2 = s.cz() * s.cz() - s.radius() * s.radius();
            if (s.cz() > image.distance() && r2 > 0.0) {
                const double projectedRadius =
                    s.radius() * image.distance() / std::sqrt(r2);
                std::cout << "        命中圆半径约 " << projectedRadius << " 像素\n";
            }
        }
    }
    catch (const std::exception& e) {
        // Settings / Raytracer / Sphere / Bitmap 的参数校验都会抛
        // std::invalid_argument，这里统一兜住并打印原因。
        std::cerr << "错误：" << e.what() << "\n";
        return 1;
    }

    return 0;
}
