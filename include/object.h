// ============================================================================
// object.h —— 场景中「物体」的抽象基类 Object，以及球体 Sphere
//
// 设计目标：把「场景里有什么形状、这个形状和射线交在哪里」和「怎么渲染」分开。
//   Object    —— 抽象基类，只规定两件事：自己能算出与射线的最近交点、自己有颜色。
//                它不认识位图，也不认识摄像机，所以加新形状时完全不用碰渲染代码。
//   Sphere    —— 目前唯一的派生类。
//
// 以后想加长方体、平面等形状，就在自己的文件里（如 box.h / box.cpp）继承 Object，
// 实现 intersect() 与 color() 两个函数，然后 Raytracer::add() 加进场景即可，
// raytracer.h / raytracer.cpp / bitmap.h / bitmap.cpp 都不需要改一个字节。
// 这就是「对扩展开放、对修改关闭」。
//
// 分工的边界（重要）：
//   Object   只管「几何与外观」—— 交在哪里（intersect）、是什么颜色（color）。
//   Raytracer 只管「怎么问」—— 为每个像素发射线、在所有物体里挑最近的那个、
//             把颜色写进位图。
//   所以基类里刻意不放「把颜色画进位图」这类函数：一来形状不该认识 Bitmap，
//   二来以后要做光照，颜色得靠交点位置和法线算出来，让形状自己涂像素会立刻卡死。
// ============================================================================

#ifndef OBJECT_H
#define OBJECT_H

#include "geometry.h"   // 需要用到 Ray 与 Vector

#include <cstdint>
#include <optional>

// ============================================================================
// 类 Object —— 场景中一个物体的抽象基类
//
//   纯接口类：既不能直接创建，也不能拷贝赋值（有虚函数、语义上也不该被切片）。
//   两个纯虚函数就是「一个物体必须能回答的两个问题」：
//       intersect(ray) —— 这条射线和我在哪里相交？
//       color()        —— 我是什么颜色？
// ============================================================================
class Object {
public:
    // 作用：虚析构函数。
    // 参数：无
    // 返回：无
    // 说明：必须有，而且必须虚。Raytracer 用 unique_ptr<Object> 持有派生类对象，
    //       通过基类指针删除时，非虚析构会导致派生类的成员不被析构（未定义行为）。
    virtual ~Object();

    // 作用：求射线与本物体的最近交点。
    // 参数：ray —— 待求交的射线
    // 返回：命中时返回交点在射线上的参数 t（t > 0 表示交点在射线起点前方）；
    //       射线与本物体不相交、或交点全在射线起点后方时返回 std::nullopt
    // 说明：纯虚函数，派生类必须实现。做成 const，因为求交不能改动物体。
    virtual std::optional<double> intersect(const Ray& ray) const = 0;

    // 作用：读取物体的颜色。
    // 参数：无
    // 返回：3 元素数组 {红, 绿, 蓝} 的引用
    // 说明：返回数组引用而不是指针，是为了保持「3 元素数组」这个类型不被退化掉，
    //       可以直接交给 Bitmap::setPixel()（它要的正是 const uint8_t (&)[3]）。
    virtual const uint8_t (&color() const)[3] = 0;

protected:
    // 作用：供派生类调用的构造函数，只负责记下颜色。
    // 参数：rgb —— 颜色 {红, 绿, 蓝}，每个分量取值 0~255
    // 返回：无
    // 说明：声明成 protected，外部无法直接创建 Object（它本来也是抽象的）。
    explicit Object(const uint8_t (&rgb)[3]);

    // 作用：校验一个参数是正数，把「必须为正」这条约束挡在派生类对象建立之前。
    // 参数：value —— 待校验的值（半径、半边长等尺寸都走这里）
    //       what  —— 出错提示里用的中文名称，如 "半径"
    // 返回：value 本身，校验通过时原样返回，方便直接写在成员初始化列表里
    // 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
    // 说明：放在基类里让所有形状共用（都要校验尺寸为正），派生类构造函数可以
    //       在初始化列表里调用它，这样非法对象根本建立不起来，后续求交不必再检查。
    static double checkPositive(double value, const char* what);

    uint8_t color_[3];   // 物体的颜色 {红, 绿, 蓝}
};

// ============================================================================
// 类 Sphere —— 球体：位置、半径、颜色
// ============================================================================
class Sphere : public Object {
public:
    // 作用：构造一个球体。
    // 参数：center —— 球心位置
    //       radius —— 半径，必须 > 0
    //       rgb    —— 颜色，3 元素数组 {红, 绿, 蓝}
    // 返回：无
    // 异常：radius <= 0（含 NaN）时抛 std::invalid_argument。异常在成员初始化阶段
    //       就抛出，所以对象不会被建立，后续求交时不必再检查半径是否合法。
    Sphere(const Vector& center, double radius, const uint8_t (&rgb)[3]);

    // 作用：求射线与球面的最近交点。
    // 参数：ray —— 待求交的射线（方向可以不归一化）
    // 返回：命中返回最近交点的参数 t（t > 0）；否则返回 std::nullopt
    // 说明：判别式 delta = b*b - 4*a*c，delta < 0 表示直线与球面不相交。
    //       注意 delta >= 0 只说明「直线」穿过球，还要看根是否为正，
    //       否则球整个在摄像机背后也会被误判成可见。
    std::optional<double> intersect(const Ray& ray) const override;

    // 作用：读取球的颜色。
    // 参数：无
    // 返回：3 元素数组 {红, 绿, 蓝} 的引用
    const uint8_t (&color() const)[3] override { return color_; }

    // 作用：读取球心位置。
    // 参数：无
    // 返回：球心坐标
    const Vector& center() const { return center_; }

    // 作用：读取半径。
    // 参数：无
    // 返回：半径，恒为正数
    double radius() const { return radius_; }

private:
    Vector center_;   // 球心
    double radius_;   // 半径
};

#endif // OBJECT_H
