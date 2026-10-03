// ============================================================================
// raytracer.cpp —— Vector / Ray / Sphere / Raytracer 的实现
//
// 只使用 C++ 标准库：<cmath> 开平方、<stdexcept> 抛异常、<string> 拼错误信息。
//
// 本节用到的数学（整个文件都依赖它）：
//
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

#include "raytracer.h"

#include <cmath>
#include <stdexcept>
#include <string>

// ============================================================================
// 类 Vector
// ============================================================================

/**
 * 作用：用三个分量构造向量。
 * 参数：x, y, z —— 三个分量
 * 返回：无
 */
Vector::Vector(double x, double y, double z) : x_(x), y_(y), z_(z) {}

/**
 * 作用：两个向量相加，对应分量分别相加。
 * 参数：other —— 被加的那个向量
 * 返回：this + other
 */
Vector Vector::operator+(const Vector& other) const
{
    return Vector(x_ + other.x_, y_ + other.y_, z_ + other.z_);
}

/**
 * 作用：两个向量相减，对应分量分别相减。
 * 参数：other —— 被减的那个向量
 * 返回：this - other
 */
Vector Vector::operator-(const Vector& other) const
{
    return Vector(x_ - other.x_, y_ - other.y_, z_ - other.z_);
}

/**
 * 作用：把向量按比例放缩，三个分量同乘一个系数。
 * 参数：k —— 放缩系数（可以为负，表示同时反向）
 * 返回：this * k
 */
Vector Vector::operator*(double k) const
{
    return Vector(x_ * k, y_ * k, z_ * k);
}

/**
 * 作用：计算两个向量的点积，即对应分量乘积之和。
 * 参数：other —— 另一个向量
 * 返回：点积，是个标量；两个向量垂直时为 0
 */
double Vector::dot(const Vector& other) const
{
    return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
}

/**
 * 作用：计算向量的模长，即 |this| = sqrt(this·this)。
 * 参数：无
 * 返回：模长，恒为非负数
 */
double Vector::length() const
{
    return std::sqrt(dot(*this));
}

// ============================================================================
// 类 Ray
// ============================================================================

/**
 * 作用：用起点和方向构造一条射线。
 * 参数：origin    —— 起点
 *       direction —— 方向（允许未归一化，但必须是零向量以外）
 * 返回：无
 * 说明：这里不校验 direction 是否为零向量：零方向会让 a = d·d = 0，
 *       求根时出现除以 0。本节的方向来自像素坐标，dz 恒等于 D > 0，
 *       不可能为零，所以把校验留在调用方能保证的地方，不在这里付代价。
 */
Ray::Ray(const Vector& origin, const Vector& direction)
    : origin_(origin), direction_(direction) {}

/**
 * 作用：求射线上参数为 t 的那个点。
 * 参数：t —— 参数值，t = 0 是起点，t = 1 是起点沿方向走一个「方向向量」远
 * 返回：origin + t * direction
 */
Vector Ray::at(double t) const
{
    return origin_ + direction_ * t;
}

// ============================================================================
// 类 Sphere / Raytracer 共用的参数校验
// ============================================================================

namespace {

/**
 * 作用：校验一个参数是正数，把「必须为正」这条约束挡在对象建立之前。
 *       这一步不能省：成员按声明顺序初始化，如果先按非法值算出坐标、
 *       甚至去分配内存，就会先出问题，根本轮不到检查。
 *       写在匿名 namespace 里，外部看不到，也不会和其他文件的同名函数冲突。
 * 参数：value —— 待校验的值（半径 R、宽、高、距离 D 都走这里）
 *       what  —— 出错提示里用的中文名称，如 "半径"、"宽"、"距离 D"
 * 返回：value 本身，校验通过时原样返回，方便直接写在成员初始化列表里
 * 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
 */
double checkPositive(double value, const char* what)
{
    if (!(value > 0.0)) {
        // 写成 !(value > 0.0) 而不是 value <= 0.0：前者连 NaN 也一并挡住
        throw std::invalid_argument(std::string("raytracer: ") + what + " 必须为正数");
    }
    return value;
}

} // namespace

// ============================================================================
// 类 Sphere
// ============================================================================

/**
 * 作用：构造一个球体。
 * 参数：center —— 球心位置
 *       radius —— 半径，必须 > 0
 *       rgb    —— 颜色 {红, 绿, 蓝}
 * 返回：无
 * 异常：radius <= 0 时抛 std::invalid_argument，异常在成员初始化阶段就抛出，
 *       所以对象不会被建立，后续求交时不必再检查半径是否合法
 */
Sphere::Sphere(const Vector& center, double radius, const uint8_t (&rgb)[3])
    : center_(center), radius_(checkPositive(radius, "半径"))
{
    for (int i = 0; i < 3; ++i) {
        color_[i] = rgb[i];
    }
}

// ============================================================================
// 类 Raytracer
// ============================================================================

/**
 * 作用：构造一台光线投射器，设定投影平面尺寸、摄像机到平面的距离 D 与背景色。
 *       此时场景是空的，还没有任何球。
 * 参数：width, height —— 位图尺寸（像素），必须为正整数
 *       distance —— 摄像机到投影平面的距离 D，必须 > 0
 *       background —— 背景色 {红, 绿, 蓝}
 * 返回：无
 * 异常：宽高不是正整数、或 distance 不是正数时抛 std::invalid_argument
 */
Raytracer::Raytracer(int width, int height, double distance, const uint8_t (&background)[3])
    : width_(static_cast<int>(checkPositive(width, "宽"))),
      height_(static_cast<int>(checkPositive(height, "高"))),
      distance_(checkPositive(distance, "距离 D"))
{
    for (int i = 0; i < 3; ++i) {
        background_[i] = background[i];
    }
}

/**
 * 作用：往场景里添加一个球。
 * 参数：sphere —— 要添加的球，按值传入并拷贝保存
 * 返回：无
 */
void Raytracer::addSphere(const Sphere& sphere)
{
    spheres_.push_back(sphere);
}

/**
 * 作用：把位图像素坐标换算成投影平面上的世界坐标。
 *       以位图中心对应投影平面的中心 (0, 0, D)：
 *           x = px - (width  - 1) / 2
 *           y = (height - 1) / 2 - py      ← y 方向要翻转
 *       用 (W - 1) / 2 而不是 W / 2，这样最左和最右像素关于中轴对称。
 * 参数：px —— 列号，取值 0 ~ width() - 1（调用前已保证）
 *       py —— 行号，取值 0 ~ height() - 1（调用前已保证）
 * 返回：该像素在投影平面上的世界坐标，z 恒等于 distance()
 */
Vector Raytracer::pixelToPlane(int px, int py) const
{
    const double halfW = (width_ - 1) / 2.0;
    const double halfH = (height_ - 1) / 2.0;

    // 位图 py 向下增大，世界坐标 y 向上增大，所以这里用减法把它翻过来。
    return Vector(px - halfW, halfH - py, distance_);
}

/**
 * 作用：求射线与球面的最近交点，并用判别式 delta = b*b - 4*a*c 判断是否相交。
 * 参数：ray    —— 射线，方向可以不归一化
 *       sphere —— 被求交的球
 *       outT   —— 输出参数：命中时写入最近交点的参数 t
 * 返回：命中返回 true；delta < 0、或两个根都不为正（球在摄像机背后）返回 false
 */
bool Raytracer::intersectSphere(const Ray& ray, const Sphere& sphere, double& outT)
{
    // 步骤 1：把球心平移到以射线起点为原点，得到向量 oc = O - C。
    //         用 O - C 而不是 C - O，是为了让下面的 b 直接带上系数 2，
    //         与常见的 a*t^2 + b*t + c = 0（b = 2*(O-C)·d）形式一致。
    const Vector oc = ray.origin() - sphere.center();

    // 步骤 2：算出二次方程 a*t^2 + b*t + c = 0 的三个系数。
    //         a = d·d 恒为正（方向向量非零），所以不用担心退化成一次方程。
    const double a = ray.direction().dot(ray.direction());
    const double b = 2.0 * oc.dot(ray.direction());
    const double c = oc.dot(oc) - sphere.radius() * sphere.radius();

    // 步骤 3：判别式。本节约定 delta >= 0 就算命中。
    const double delta = b * b - 4.0 * a * c;
    if (delta < 0.0) {
        return false;                       // 直线与球面不相交：这条射线看到的是背景
    }

    // 步骤 4：开方求两个根。delta == 0 时两根重合（相切），取到的就是切点。
    const double sqrtDelta = std::sqrt(delta);
    const double t0 = (-b - sqrtDelta) / (2.0 * a);   // 离摄像机较近的交点
    const double t1 = (-b + sqrtDelta) / (2.0 * a);   // 较远的交点

    // 步骤 5：取最近的那个正根。
    //         直线与球面相交不等于能看到球：如果两个根都不为正，球整个在摄像机
    //         背后，此时 delta > 0 却什么都没挡住，仍应算作没命中。
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

/**
 * 作用：求某个像素命中的球，即沿射线找出最近的、被挡在前面的球。
 * 参数：px —— 列号，取值 0 ~ width() - 1
 *       py —— 行号，取值 0 ~ height() - 1
 * 返回：指向最近命中球的指针；没有任何球被命中时返回 nullptr
 */
const Sphere* Raytracer::tracePixel(int px, int py) const
{
    // 步骤 1：像素 P 在世界坐标里的位置，射线起点是摄像机 O(0,0,0)、
    //         方向 d = P - O = P。
    const Vector direction = pixelToPlane(px, py);
    const Ray ray(Vector(0.0, 0.0, 0.0), direction);

    // 步骤 2：逐个球求交，留下 t 最小（离摄像机最近）的那个。
    //         本节不区分正反面、也不做遮挡剔除，球很少时线性扫描足够了。
    const Sphere* nearest = nullptr;
    double nearestT = 0.0;

    for (const Sphere& sphere : spheres_) {
        double t = 0.0;
        if (!intersectSphere(ray, sphere, t)) {
            continue;
        }
        if (nearest == nullptr || t < nearestT) {
            nearest = &sphere;
            nearestT = t;
        }
    }

    return nearest;
}

/**
 * 作用：渲染整个场景。对每个像素打一条射线，命中就涂球色、否则涂背景色。
 * 参数：无
 * 返回：渲染好的位图，调用方拿到后自行决定是否写盘
 * 异常：不抛异常（构造时已校验过尺寸，位图构造不会再失败）
 */
Bitmap Raytracer::render() const
{
    // 先整张填成背景色，之后命中的像素才被改写，省掉逐像素判断背景的分支。
    Bitmap bitmap(width_, height_, background_);

    for (int py = 0; py < height_; ++py) {
        for (int px = 0; px < width_; ++px) {
            const Sphere* hit = tracePixel(px, py);
            if (hit != nullptr) {
                // 只有命中的像素才需要写一次，空白区域保持构造时的背景色。
                // color() 返回的是球的 3 元素颜色数组，正好是 setPixel 要的形状。
                bitmap.setPixel(px, py, hit->color());
            }
        }
    }

    return bitmap;
}
