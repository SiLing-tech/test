// ============================================================================
// raytracer.h —— 光线投射（ray casting）场景的封装
//
// 这一节把「把位图当投影平面、打出射线、和球求交」的整套逻辑从 main.cpp 里
// 独立出来，并用类包装。与 bitmap.h 的分工是：
//   Bitmap    —— 只管「一张内存中的位图」，不关心图上画的是什么；
//   Raytracer —— 只管「场景里有什么、每个像素看到什么颜色」，通过 Bitmap 输出。
// 所以 Raytracer 的渲染结果是 return 一个 Bitmap，而不是自己去写文件。
//
// 几何设定（与坐标系约定）：
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
//     于是每个像素对应一条射线 OP，起点是原点、方向是 d = P - O = P。
//     把 d 写成 (dx, dy, D)，那么在 t = 1 时 O + t*d 恰好落在平面上，
//     所以 t 的尺度与像素坐标直接对应，无需归一化。
//
// 用法示例：
//     const uint8_t background[3] = {0, 0, 0};
//     Raytracer tracer(640, 480, 500.0, background);   // W、H、距离 D、背景色
//     const uint8_t orange[3] = {255, 80, 0};
//     tracer.addSphere(Sphere(Vector(0, 0, 800), 150.0, orange));
//     const Bitmap image = tracer.render();            // 每个像素打一条射线
// ============================================================================

#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "bitmap.h"

#include <cstdint>
#include <vector>

// ============================================================================
// 类 Vector —— 三维向量（点也算向量：从原点指向该点的向量）
//
//   只用得上光线投射需要的最小操作：加减、数乘、点积、求模长。
//   刻意做成不可变对象（所有运算都返回新对象），避免不小心改到场景数据。
// ============================================================================
class Vector {
public:
    // 作用：用三个分量构造向量。
    // 参数：x, y, z —— 三个分量
    // 返回：无
    Vector(double x, double y, double z);

    // 作用：读取三个分量。
    // 参数：无
    // 返回：对应的分量值
    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }

    // 作用：两个向量相加，得到新向量。
    // 参数：other —— 被加的那个向量
    // 返回：this + other
    Vector operator+(const Vector& other) const;

    // 作用：两个向量相减，得到新向量。
    // 参数：other —— 被减的那个向量
    // 返回：this - other
    Vector operator-(const Vector& other) const;

    // 作用：把向量按比例放缩，得到新向量。
    // 参数：k —— 放缩系数
    // 返回：this * k
    Vector operator*(double k) const;

    // 作用：计算两个向量的点积（内积），也就是 |a||b|cos(夹角)。
    // 参数：other —— 另一个向量
    // 返回：点积，是个标量
    double dot(const Vector& other) const;

    // 作用：计算向量的模长（长度）。
    // 参数：无
    // 返回：|this|，恒为非负数
    double length() const;

private:
    double x_;  // 分量 x
    double y_;  // 分量 y
    double z_;  // 分量 z
};

// ============================================================================
// 类 Ray —— 一条射线，参数方程 P(t) = origin + t * direction
//
//   t 是参数而不是真实距离：方向向量没归一化时 |t| 不等于长度。
//   本节的射线方向就是像素的世界坐标减去原点，故意保持未归一化 ——
//   这样 t = 1 恰好对应投影平面上的那个像素，判断「交点在摄像机前方」
//   只需要看 t 的正负，不必再做一次开方。
// ============================================================================
class Ray {
public:
    // 作用：用起点和方向构造一条射线。
    // 参数：origin    —— 起点
    //       direction —— 方向（允许未归一化，但必须是零向量以外）
    // 返回：无
    Ray(const Vector& origin, const Vector& direction);

    // 作用：读取射线的起点。
    // 参数：无
    // 返回：起点坐标
    const Vector& origin() const { return origin_; }

    // 作用：读取射线的方向。
    // 参数：无
    // 返回：方向向量
    const Vector& direction() const { return direction_; }

    // 作用：求射线上参数为 t 的那个点，即 origin + t * direction。
    // 参数：t —— 参数值
    // 返回：该点坐标
    Vector at(double t) const;

private:
    Vector origin_;     // 起点
    Vector direction_;  // 方向
};

// ============================================================================
// 类 Sphere —— 场景中的一个球体：位置、半径、颜色
//
//   只保存几何与外观，不含任何渲染逻辑（「看到什么颜色」是 Raytracer 的事）。
//   构造时校验半径为正，异常在对象建立之前抛出，所以后续使用不必再检查。
// ============================================================================
class Sphere {
public:
    // 作用：构造一个球体。
    // 参数：center —— 球心位置
    //       radius —— 半径，必须 > 0
    //       rgb    —— 颜色，3 元素数组 {红, 绿, 蓝}，每个分量取值 0~255
    // 返回：无
    // 异常：radius <= 0 时抛 std::invalid_argument
    Sphere(const Vector& center, double radius, const uint8_t (&rgb)[3]);

    // 作用：读取球心位置。
    // 参数：无
    // 返回：球心坐标
    const Vector& center() const { return center_; }

    // 作用：读取半径。
    // 参数：无
    // 返回：半径，恒为正数
    double radius() const { return radius_; }

    // 作用：读取球的颜色。
    // 参数：无
    // 返回：3 元素数组 {红, 绿, 蓝} 的引用；返回引用而不是指针，是为了保持
    //       「3 元素数组」这个类型不被退化掉，可以直接交给 Bitmap::setPixel()
    //       （它要的正是 const uint8_t (&)[3]）
    const uint8_t (&color() const)[3] { return color_; }

private:
    Vector  center_;      // 球心
    double  radius_;      // 半径
    uint8_t color_[3];    // 颜色 {红, 绿, 蓝}
};

// ============================================================================
// 类 Raytracer —— 一台固定机位的「摄像机 + 场景」
//
//   摄像机固定在原点朝 +z 看，投影平面在 z = D 处、尺寸就是位图尺寸 W x H。
//   场景里可以放任意多个球，render() 会为每个像素打一条射线，取最近的命中球，
//   命中就涂那个球的颜色，都没命中就涂背景色。
//
//   因为摄像机位置和投影平面在构造时就定死了（对应本节「摄像机位于平面中点
//   正上方」的设定），宽、高、距离 D 之后不再改变，所以只有只读的访问器；
//   场景内容则通过 addSphere() 在渲染前任意添加。
// ============================================================================
class Raytracer {
public:
    // 作用：构造一台光线投射器，设定投影平面（即位图）的尺寸、摄像机到平面的
    //       直线距离 D，以及没有任何球被命中时使用的背景色。此时场景是空的。
    // 参数：width      —— 位图宽（像素），即投影平面在 x 方向的像素数，必须 > 0
    //       height     —— 位图高（像素），即投影平面在 y 方向的像素数，必须 > 0
    //       distance   —— 摄像机到投影平面的垂直距离 D，必须 > 0
    //       background —— 背景色，3 元素数组 {红, 绿, 蓝}
    // 返回：无
    // 异常：width/height 不是正整数、或 distance 不是正数时抛 std::invalid_argument
    Raytracer(int width, int height, double distance, const uint8_t (&background)[3]);

    // 作用：读取投影平面的宽（水平方向像素数）。
    // 参数：无
    // 返回：宽，恒为正整数
    int width() const { return width_; }

    // 作用：读取投影平面的高（竖直方向像素数）。
    // 参数：无
    // 返回：高，恒为正整数
    int height() const { return height_; }

    // 作用：读取摄像机到投影平面的距离 D。
    // 参数：无
    // 返回：距离，恒为正数
    double distance() const { return distance_; }

    // 作用：读取背景色。
    // 参数：无
    // 返回：3 元素数组 {红, 绿, 蓝}
    const uint8_t* background() const { return background_; }

    // 作用：往场景里添加一个球。
    // 参数：sphere —— 要添加的球（按值传入，内部拷贝一份保存）
    // 返回：无
    // 说明：渲染时若有多个球被同一条射线命中，取 t 较小（离摄像机较近）的那个；
    //       这里不做遮挡剔除等优化，球数量很少时逐条射线线性扫描足够了。
    void addSphere(const Sphere& sphere);

    // 作用：场景里的球体个数。
    // 参数：无
    // 返回：已添加的球数
    std::size_t sphereCount() const { return spheres_.size(); }

    // 作用：渲染整个场景，为每个像素打一条射线并决定它的颜色。
    // 参数：无
    // 返回：一张宽 width()、高 height() 的位图；调用方拿到后自行决定是否写盘
    // 异常：不抛异常（构造时已经把非法尺寸挡掉了）
    Bitmap render() const;

private:
    // 作用：求射线与球面的最近交点，并按本节约定用判别式
    //       delta = b*b - 4*a*c 判断是否相交。
    // 参数：ray    —— 射线（方向可以不归一化）
    //       sphere —— 被求交的球
    //       outT   —— 输出参数：命中时写入最近交点的参数 t
    // 返回：命中返回 true；delta < 0（与球面不相交）、或两个根都 <= 0
    //       （球整个在摄像机背后）返回 false，此时 outT 的值无意义
    // 说明：做成静态成员而不是自由函数，是为了不把求交的数学细节暴露给外部。
    static bool intersectSphere(const Ray& ray, const Sphere& sphere, double& outT);

    // 作用：把位图像素坐标换算成投影平面上的世界坐标。
    // 参数：px —— 列号，取值 0 ~ width() - 1（调用前已保证）
    //       py —— 行号，取值 0 ~ height() - 1（调用前已保证）
    // 返回：该像素在投影平面上的世界坐标，z 恒等于 distance()
    Vector pixelToPlane(int px, int py) const;

    // 作用：求某个像素命中的球：沿射线找出最近的、被挡在前面的球。
    // 参数：px —— 列号，取值 0 ~ width() - 1
    //       py —— 行号，取值 0 ~ height() - 1
    // 返回：指向场景中最近命中球的指针；没有任何球被命中时返回 nullptr。
    //       返回的指针指向本对象内部 spheres_ 里的元素，生命周期不短于本对象。
    //       故意返回「球」而不是「颜色」：颜色交给调用方用 color() 取，
    //       这样「没命中」这件事由 nullptr 明确表达，不必靠和背景色比较指针。
    const Sphere* tracePixel(int px, int py) const;

    int                  width_;        // 投影平面宽（像素），构造后不再改变
    int                  height_;       // 投影平面高（像素），构造后不再改变
    double               distance_;     // 摄像机到投影平面的距离 D，构造后不再改变
    uint8_t              background_[3]; // 背景色
    std::vector<Sphere>  spheres_;      // 场景中的球体，渲染时按添加顺序扫描
};

#endif // RAYTRACER_H
