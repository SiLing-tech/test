// ============================================================================
// main.cpp —— 用光线投射（ray casting）把一张位图当作投影平面来渲染球体
//
// 本节的几何设定（与坐标系约定）：
//
//     世界坐标：右手系，x 向右、y 向上、z 指向观察者前方（屏幕里侧）。
//
//     摄像机位于世界原点 O(0, 0, 0)，朝 +z 方向看。
//     投影平面就是那张位图：一块宽高与像素数 1:1 的矩形，中心在 (0, 0, D)，
//     即「平面中点的正上方 D 处」是摄像机，D 就是摄像机的直线距离。
//     位图上像素 (px, py) 的世界坐标由平面中心加上偏移得到：
//         P = (px - (W - 1) / 2,  (H - 1) / 2 - py,  D)
//     注意 y 方向要翻转：位图里 py 向下增大，而世界坐标里 y 向上增大。
//
//     于是每个像素对应一条射线 OP，方向 d = P - O = P（起点是原点）。
//     把 d 写成 (dx, dy, D)，那么在 t = 1 时 O + t*d 恰好落在平面上，
//     所以 t 的尺度与像素坐标直接对应，无需归一化。
//
//     射线与球心 C、半径 R 的球求交，代入 |O + t*d - C|^2 = R^2 整理成
//         a = d·d,  b = 2 * (O - C)·d,  c = |O - C|^2 - R^2
//         delta = b * b - 4 * a * c
//     delta > 0 有两个交点（射线穿过球体）、delta == 0 相切，
//     按本节约定 delta >= 0 即视为命中，把该像素涂成球的颜色 color；
//     否则说明这条射线没碰到球，涂背景色。
//
// 编译：g++ -std=c++17 -Wall -Wextra -pedantic -O2 -o bitmap_demo.exe main.cpp bitmap.cpp
// 运行：bitmap_demo.exe，会在当前目录生成 sphere.bmp
// ============================================================================

#include "bitmap.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

// ============================================================================
// 场景参数（想换效果直接改这几个常量即可）
// ============================================================================

// 位图分辨率：投影平面上「一个像素」的疏密就由它决定
const int kWidth  = 640;
const int kHeight = 480;

// 摄像机到投影平面的直线距离 D
const double kDistance = 500.0;

// 背景色 {红, 绿, 蓝}
const uint8_t kBackground[3] = {0, 0, 0};

// ============================================================================
// 射线与球面求交
// ============================================================================

/**
 * 作用：求射线 O + t * d 与球面的最近交点，用判别式 delta 判断是否相交。
 * 参数：ox, oy, oz —— 射线起点（本节是原点）
 *       dx, dy, dz —— 射线方向，允许未归一化
 *       cx, cy, cz —— 球心
 *       radius     —— 半径，必须 > 0
 *       outT       —— 输出参数：命中时写入最近交点的 t
 * 返回：命中返回 true；delta < 0、或球在射线背后（两个根都不为正）返回 false
 * 异常：radius <= 0 时抛 std::invalid_argument
 */
bool intersectSphere(double ox, double oy, double oz,
                     double dx, double dy, double dz,
                     double cx, double cy, double cz, double radius,
                     double& outT)
{
    if (radius <= 0.0) {
        throw std::invalid_argument("intersectSphere: 半径必须为正数");
    }

    // 步骤 1：把球心平移到以射线起点为原点，得到向量 oc = O - C。
    //         用 O - C 而不是 C - O，是为了让下面的 b 直接带上系数 2，
    //         与常见的 a*t^2 + b*t + c = 0（b = 2*(O-C)·d）形式一致。
    const double ocx = ox - cx;
    const double ocy = oy - cy;
    const double ocz = oz - cz;

    // 步骤 2：算出二次方程 a*t^2 + b*t + c = 0 的三个系数。
    //         a = d·d 恒为正（方向向量非零），所以不用担心退化成一次方程。
    const double a = dx * dx + dy * dy + dz * dz;
    const double b = 2.0 * (ocx * dx + ocy * dy + ocz * dz);
    const double c = ocx * ocx + ocy * ocy + ocz * ocz - radius * radius;

    // 步骤 3：判别式。本节约定 delta >= 0 就算命中。
    const double delta = b * b - 4.0 * a * c;
    if (delta < 0.0) {
        return false;                       // 射线与球面不相交：这条射线看到的是背景
    }

    // 步骤 4：开方求两个根。delta == 0 时两根重合（相切），取到的就是切点。
    const double sqrtDelta = std::sqrt(delta);
    const double t0 = (-b - sqrtDelta) / (2.0 * a);   // 离摄像机较近的交点
    const double t1 = (-b + sqrtDelta) / (2.0 * a);   // 较远的交点

    // 步骤 5：取最近的那个正根。
    //         直线与球面相交不等于能看到球：如果两个根都为负，球整个在摄像机背后，
    //         此时 delta > 0 却什么都没挡住，仍应涂背景色。
    if (t0 > 0.0) {
        outT = t0;
    } else if (t1 > 0.0) {
        // 摄像机在球内部：近根为负，退而取远根
        outT = t1;
    } else {
        return false;
    }

    return true;
}

// ============================================================================
// 渲染：对每个像素打一条射线
// ============================================================================

/**
 * 作用：按本节设定渲染球体并保存为 BMP。
 * 参数：filePath —— 输出路径
 *       width, height —— 位图尺寸（即投影平面的像素数），必须 > 0
 *       distance —— 摄像机到平面的距离 D，必须 > 0
 * 返回：成功返回 true；参数非法或写盘失败返回 false
 * 异常：不抛异常，异常一律转成返回值 false
 */
bool renderSphereScene(const std::string& filePath, int width, int height, double distance)
{
    try {
        if (!(distance > 0.0)) {
            throw std::invalid_argument("renderSphereScene: 距离 D 必须为正数");
        }

        // ---------- 初始化球体：半径 R、颜色 color、球心位置 ----------
        const double kRadius = 150.0;                 // R
        const uint8_t kSphereColor[3] = {255, 80, 0}; // color {红, 绿, 蓝}
        const double kSphereCenter[3] = {0.0, 0.0, 800.0}; // 球心放在中轴线上，平面之后

        // 构造时整张图先填背景色，之后命中的像素才被改写。
        Bitmap bitmap(width, height, kBackground);

        // 像素坐标 → 世界坐标的偏移：把位图中心平移到世界原点所在的竖轴上。
        // 用 (width - 1) / 2.0 而不是 width / 2.0，这样最左和最右像素关于中轴对称。
        const double halfW = (width - 1) / 2.0;
        const double halfH = (height - 1) / 2.0;

        // 命中边界 q = R（球心到射线的垂直距离等于半径）在投影平面上是一个圆：
        //     x^2 + y^2 = R^2 * D^2 / (Cz^2 - R^2)
        // 所以命中区域的半径是 R*D / sqrt(Cz^2 - R^2)，下面拿来和 hitCount 对账。
        const double r2 = kSphereCenter[2] * kSphereCenter[2] - kRadius * kRadius;
        const double projectedRadius =
            (r2 > 0.0) ? kRadius * distance / std::sqrt(r2) : 0.0;
        const double expectedPixels = 3.14159265358979323846 * projectedRadius * projectedRadius;

        // 逐行逐像素投射射线，统计命中数以便检查结果（也方便和投影半径对账）
        long long hitCount = 0;
        for (int py = 0; py < height; ++py) {
            for (int px = 0; px < width; ++px) {
                // 步骤 1：算出像素 P 在投影平面上的世界坐标。
                //         y 要翻转：位图 py 向下增大，世界 y 向上增大。
                const double wx = px - halfW;
                const double wy = halfH - py;
                const double wz = distance;

                // 步骤 2：射线起点是摄像机 O(0,0,0)，方向 d = P - O = P。
                const double dx = wx;
                const double dy = wy;
                const double dz = wz;

                // 步骤 3：与球求交；命中就涂球色，否则保持构造时的背景色。
                double t = 0.0;
                if (intersectSphere(0.0, 0.0, 0.0, dx, dy, dz,
                                    kSphereCenter[0], kSphereCenter[1], kSphereCenter[2],
                                    kRadius, t)) {
                    bitmap.setPixel(px, py, kSphereColor);
                    ++hitCount;
                }
            }
        }

        // ---------- 写盘 ----------
        if (!bitmap.save(filePath)) {
            std::cerr << "保存失败：" << filePath << "\n";
            return false;
        }

        // ---------- 打印一行摘要 ----------
        std::cout << "已生成位图：" << filePath
                  << "  " << bitmap.width() << " x " << bitmap.height() << "\n"
                  << "摄像机 O(0,0,0)，投影平面 z = " << distance << "\n"
                  << "球体：R = " << kRadius
                  << "  球心 (" << kSphereCenter[0] << ", " << kSphereCenter[1]
                  << ", " << kSphereCenter[2] << ")"
                  << "  颜色 RGB(" << static_cast<int>(kSphereColor[0]) << ", "
                                   << static_cast<int>(kSphereColor[1]) << ", "
                                   << static_cast<int>(kSphereColor[2]) << ")\n"
                  << "背景色 RGB(" << static_cast<int>(kBackground[0]) << ", "
                                   << static_cast<int>(kBackground[1]) << ", "
                                   << static_cast<int>(kBackground[2]) << ")\n"
                  << "命中 " << hitCount << " 个像素（投影圆半径约 "
                  << projectedRadius << " 像素，面积约 " << expectedPixels << "）\n";
    }
    catch (const std::exception& e) {
        // Bitmap 构造可能因宽高非法抛 std::invalid_argument，
        // setPixel 的坐标由循环变量保证不会越界，但这里仍统一兜住。
        std::cerr << "错误：" << e.what() << "\n";
        return false;
    }

    return true;
}

/**
 * 作用：程序入口。按本节设定渲染一张「球 + 背景」的位图并写盘。
 * 参数：无（不使用命令行参数；如需从命令行读分辨率/距离，可改成
 *       int main(int argc, char* argv[]) 再用 std::stod 解析）
 * 返回：成功返回 0，失败返回 1
 */
int main()
{
    const std::string path = "sphere.bmp";
    if (!renderSphereScene(path, kWidth, kHeight, kDistance)) {
        return 1;
    }
    return 0;
}
