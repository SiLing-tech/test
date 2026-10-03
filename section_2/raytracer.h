// ============================================================================
// raytracer.h —— 光线投射（ray casting）里的「摄像机 + 场景」
//
// 三个头文件的分工：
//   Bitmap    —— 只管「一张内存中的位图」，不关心图上画的是什么；
//   Object    —— 只管「一个物体长什么样、射线和它交在哪里」（object.h）；
//   Raytracer —— 只管「场景里有哪些物体、每个像素看到什么颜色」，通过 Bitmap 输出。
// 基础几何量 Vector / Ray 放在 geometry.h，三边都依赖它。
// 依赖关系是干净的单向链：
//     bitmap.h  →  geometry.h  →  object.h  →  raytracer.h  →  main.cpp
//
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
//     const uint8_t orange[3] = {255, 80, 0};
//     Raytracer tracer(640, 480, 500.0, background);      // W、H、距离 D、背景色
//     tracer.addSphere(Sphere(Vector(0, 0, 800), 150.0, orange));
//     const Bitmap image = tracer.render();               // 每个像素打一条射线
//
//     想加别的形状（长方体等），写一个继承 Object 的类，
//     调用 tracer.add(std::make_unique<Box>(...)) 即可，本文件无需修改。
// ============================================================================

#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "bitmap.h"
#include "object.h"

#include <cstdint>
#include <memory>
#include <vector>

// ============================================================================
// 类 Raytracer —— 一台固定机位的「摄像机 + 场景」
//
//   摄像机固定在原点朝 +z 看，投影平面在 z = D 处、尺寸就是位图尺寸 W x H。
//   场景里可以放任意多个物体（都是 Object 的派生类），render() 会为每个像素打
//   一条射线，用多态调用 obj->intersect() 取最近的命中物体，命中就涂那个物体的
//   颜色，都没命中就涂背景色。
//
//   因为摄像机位置和投影平面在构造时就定死了（对应本节「摄像机位于平面中点
//   正上方」的设定），宽、高、距离 D 之后不再改变，所以只有只读的访问器；
//   场景内容则通过 add() / addSphere() 在渲染前任意添加。
//
//   物体以 std::unique_ptr<Object> 持有：形状是多态的，必须放在堆上通过指针访问，
//   否则放进容器会发生「对象切片」（派生类那部分被砍掉，只剩基类）。
//   选 unique_ptr 而不是 shared_ptr，是因为场景独占这些物体：所有权唯一、
//   没有引用计数开销、也不会出现「两个场景偷偷共用同一个物体」。
//   代价是本类因此不可拷贝（unique_ptr 不可拷贝），只可移动，见下面的声明。
// ============================================================================
class Raytracer {
public:
    // 作用：构造一台光线投射器，设定投影平面（即位图）的尺寸、摄像机到平面的
    //       直线距离 D，以及没有任何物体被命中时使用的背景色。此时场景是空的。
    // 参数：width      —— 位图宽（像素），即投影平面在 x 方向的像素数，必须 > 0
    //       height     —— 位图高（像素），即投影平面在 y 方向的像素数，必须 > 0
    //       distance   —— 摄像机到投影平面的垂直距离 D，必须 > 0
    //       background —— 背景色，3 元素数组 {红, 绿, 蓝}
    // 返回：无
    // 异常：width/height 不是正整数、或 distance 不是正数时抛 std::invalid_argument
    Raytracer(int width, int height, double distance, const uint8_t (&background)[3]);

    // 作用：本类持有 unique_ptr，拷贝没有合理语义（要么共享物体、要么深拷贝，
    //       两者都容易出错），所以显式禁用拷贝，只保留移动。
    //       写成 = delete 而不是放任不管：让误用它的代码在编译期就报错，
    //       而不是得到一个「偷偷浅拷贝、两处共享同一批物体」的半残对象。
    Raytracer(const Raytracer&) = delete;
    Raytracer& operator=(const Raytracer&) = delete;
    Raytracer(Raytracer&&) = default;
    Raytracer& operator=(Raytracer&&) = default;

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
    // 返回：3 元素数组 {红, 绿, 蓝} 的引用
    const uint8_t (&background() const)[3] { return background_; }

    // 作用：往场景里添加一个物体，接管它的所有权。
    // 参数：object —— 指向物体的智能指针，不能为空
    // 返回：无
    // 异常：object 为空指针时抛 std::invalid_argument
    // 说明：这是「加任意形状」的通用入口；以后新增的形状直接走这里。
    void add(std::unique_ptr<Object> object);

    // 作用：往场景里添加一个球体。只是上面 add() 的便利包装，省得调用方写
    //       std::make_unique<Sphere>(...)。
    // 参数：sphere —— 要添加的球（按值传入，内部拷贝一份放到堆上）
    // 返回：无
    // 异常：sphere 参数非法时由 Sphere 构造函数抛出，此时不对场景做任何改动
    void addSphere(const Sphere& sphere);

    // 作用：场景里的物体个数。
    // 参数：无
    // 返回：已添加的物体数
    std::size_t objectCount() const { return objects_.size(); }

    // 作用：渲染整个场景，为每个像素打一条射线并决定它的颜色。
    // 参数：无
    // 返回：一张宽 width()、高 height() 的位图；调用方拿到后自行决定是否写盘
    // 异常：不抛异常（构造时已经把非法尺寸挡掉了）
    // 说明：本函数不修改场景，所以是 const，遍历物体用 const 引用即可。
    Bitmap render() const;

private:
    // 作用：把位图像素坐标换算成投影平面上的世界坐标。
    // 参数：px —— 列号，取值 0 ~ width() - 1（调用前已保证）
    //       py —— 行号，取值 0 ~ height() - 1（调用前已保证）
    // 返回：该像素在投影平面上的世界坐标，z 恒等于 distance()
    Vector pixelToPlane(int px, int py) const;

    // 作用：求某个像素命中的物体：沿射线问遍场景里的每个物体，取 t 最小
    //       （离摄像机最近）的那个。
    // 参数：px —— 列号，取值 0 ~ width() - 1
    //       py —— 行号，取值 0 ~ height() - 1
    // 返回：指向场景中最近命中物体的指针；没有任何物体被命中时返回 nullptr。
    //       返回的指针指向本对象内部 objects_ 所管理的物体，生命周期不短于本对象。
    //       故意返回「物体」而不是「颜色」：颜色交给调用方用 color() 取，
    //       这样「没命中」这件事由 nullptr 明确表达。
    // 说明：这里只做线性扫描。物体很少时足够了；将来物体成千上万，再引入
    //       包围盒层次（BVH）之类的加速结构，替换点就在这个函数里。
    const Object* tracePixel(int px, int py) const;

    int                                  width_;        // 投影平面宽（像素），构造后不再改变
    int                                  height_;       // 投影平面高（像素），构造后不再改变
    double                               distance_;     // 摄像机到投影平面的距离 D
    uint8_t                              background_[3]; // 背景色
    std::vector<std::unique_ptr<Object>> objects_;      // 场景中的物体，渲染时按添加顺序扫描
};

#endif // RAYTRACER_H
