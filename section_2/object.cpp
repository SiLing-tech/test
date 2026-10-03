// ============================================================================
// object.cpp —— Object 基类与 Sphere 的实现
//
// 本文件里的数学只有一处：射线与球面求交。
//   射线 P(t) = O + t*d 与球心 C、半径 R 的球面相交，等价于求 t 使
//       |O + t*d - C|^2 = R^2
//   把 O - C 记作 oc，展开成关于 t 的二次方程：
//       a = d·d,   b = 2 * oc·d,   c = oc·oc - R^2
//       delta = b * b - 4 * a * c
//   因为方向向量 d 不是零向量，a > 0 恒成立，所以不会退化成一次方程。
//
//   判别式的几何含义可以直接看出来：a = d·d 时
//       delta = 4 * (d·d) * (R^2 - q^2)
//   其中 q 是球心到这条射线所在直线的垂直距离。于是
//       delta > 0  直线穿过球（q < R）
//       delta == 0 直线与球相切（q == R）
//       delta < 0  直线与球不相交（q > R）
//   这正是「delta >= 0 就算命中」这条约定的来历。
// ============================================================================

#include "object.h"

#include <cmath>
#include <stdexcept>
#include <string>

// ============================================================================
// 类 Object
// ============================================================================

/**
 * 作用：虚析构函数。这里不需要释放任何资源，写成空函数体即可 ——
 *       它的意义在于「必须存在且必须虚」，让通过基类指针删除派生类对象时
 *       能正确调到派生类的析构函数。
 * 参数：无
 * 返回：无
 * 说明：必须写在 .cpp 里而不是写成 = default（写在类内也行，但放在这里便于加注释）。
 */
Object::~Object() = default;

/**
 * 作用：供派生类调用的构造函数，把颜色拷贝进基类成员。
 * 参数：rgb —— 颜色 {红, 绿, 蓝}
 * 返回：无
 */
Object::Object(const uint8_t (&rgb)[3])
{
    for (int i = 0; i < 3; ++i) {
        color_[i] = rgb[i];
    }
}

/**
 * 作用：校验一个尺寸参数是正数。
 * 参数：value —— 待校验的值
 *       what  —— 出错提示里用的中文名称
 * 返回：value 本身，便于直接写在成员初始化列表里
 * 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
 */
double Object::checkPositive(double value, const char* what)
{
    // 写成 !(value > 0.0) 而不是 value <= 0.0：前者连 NaN 也一并挡住
    if (!(value > 0.0)) {
        throw std::invalid_argument(std::string("Object: ") + what + " 必须为正数");
    }
    return value;
}

// ============================================================================
// 类 Sphere
// ============================================================================

/**
 * 作用：构造一个球体。
 * 参数：center —— 球心位置
 *       radius —— 半径，必须 > 0
 *       rgb    —— 颜色 {红, 绿, 蓝}
 * 返回：无
 * 异常：radius <= 0 时抛 std::invalid_argument，异常在成员初始化阶段抛出
 */
Sphere::Sphere(const Vector& center, double radius, const uint8_t (&rgb)[3])
    : Object(rgb),                              // 颜色交给基类保存
      center_(center),
      radius_(checkPositive(radius, "半径"))    // 校验后才赋给成员，非法时对象建立不起来
{
}

/**
 * 作用：求射线与球面的最近交点。
 * 参数：ray —— 待求交的射线（方向可以不归一化）
 * 返回：命中返回最近交点的参数 t（t > 0）；不相交或交点都在后方返回 std::nullopt
 */
std::optional<double> Sphere::intersect(const Ray& ray) const
{
    // 步骤 1：把球心平移到以射线起点为原点，得到向量 oc = O - C。
    //         用 O - C 而不是 C - O，是为了让下面的 b 直接带上系数 2，
    //         与常见的 a*t^2 + b*t + c = 0（b = 2*(O-C)·d）形式一致。
    const Vector oc = ray.origin() - center_;

    // 步骤 2：算出二次方程的三个系数。
    //         a = d·d 恒为正（方向向量非零），所以不用担心退化成一次方程。
    const double a = ray.direction().dot(ray.direction());
    const double b = 2.0 * oc.dot(ray.direction());
    const double c = oc.dot(oc) - radius_ * radius_;

    // 步骤 3：判别式。约定 delta >= 0 就算相交。
    const double delta = b * b - 4.0 * a * c;
    if (delta < 0.0) {
        return std::nullopt;            // 直线与球面不相交
    }

    // 步骤 4：开方求两个根。delta == 0 时两根重合（相切），取到的就是切点。
    const double sqrtDelta = std::sqrt(delta);
    const double t0 = (-b - sqrtDelta) / (2.0 * a);   // 离摄像机较近的交点
    const double t1 = (-b + sqrtDelta) / (2.0 * a);   // 较远的交点

    // 步骤 5：取最近的那个正根。
    //         直线与球面相交不等于能看到球：如果两个根都不为正，球整个在射线
    //         起点背后，此时 delta > 0 却什么都没挡住，仍应算作没命中。
    if (t0 > 0.0) {
        return t0;
    }
    if (t1 > 0.0) {
        return t1;                      // 起点在球内部：近根为负，退而取远根
    }
    return std::nullopt;
}
