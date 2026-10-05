// ============================================================================
// raytracer.cpp —— 类 Raytracer 的实现
//
// 本文件只做「问」的活：为每个像素构造一条射线，问遍场景里的每个物体，
// 挑出最近命中的那个，把它的颜色写进位图。
// 具体某个形状怎么和射线求交，是那个形状自己的事（见 object.cpp）。
//
// 只使用 C++ 标准库：<stdexcept> 抛异常、<string> 拼错误信息、<memory> 智能指针。
// ============================================================================

#include "raytracer.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

/**
 * 作用：校验尺寸与距离是正数，把「宽高和 D 必须为正」这条约束挡在对象建立之前。
 *       这一步不能省：成员按声明顺序初始化，如果先按非法尺寸去算像素坐标、
 *       甚至去构造位图，就会先出问题，根本轮不到检查。
 * 参数：value —— 待校验的值（传进来的宽、高或距离 D）
 *       what  —— 出错提示里用的中文名称，如 "宽"、"高"、"距离 D"
 * 返回：value 本身，校验通过时原样返回，方便直接写在成员初始化列表里
 * 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
 */
double checkPositive(double value, const char* what)
{
    // 写成 !(value > 0.0) 而不是 value <= 0.0：前者连 NaN 也一并挡住
    if (!(value > 0.0)) {
        throw std::invalid_argument(std::string("Raytracer: ") + what + " 必须为正数");
    }
    return value;
}

} // namespace

/**
 * 作用：构造一台光线投射器，设定投影平面尺寸、摄像机到平面的距离 D 与背景色。
 *       此时场景是空的，还没有任何物体。
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
 * 作用：往场景里添加一个物体，接管它的所有权。
 * 参数：object —— 指向物体的智能指针，不能为空
 * 返回：无
 * 异常：object 为空指针时抛 std::invalid_argument
 */
void Raytracer::add(std::unique_ptr<Object> object)
{
    if (object == nullptr) {
        // 提前挡住空指针，否则渲染时会解引用空指针而崩溃
        throw std::invalid_argument("Raytracer::add: 物体指针不能为空");
    }

    // 用移动而不是拷贝把所有权转进容器：unique_ptr 不可拷贝，这也正是
    // 「场景独占这些物体」在类型层面的体现。
    objects_.push_back(std::move(object));
}

/**
 * 作用：往场景里添加一个球体，是 add() 的便利包装。
 * 参数：sphere —— 要添加的球
 * 返回：无
 * 异常：sphere 参数非法时由 Sphere 构造函数抛出（此时 add 还没被调用，场景不变）
 */
void Raytracer::addSphere(const Sphere& sphere)
{
    // make_unique 在堆上构造一份拷贝；多态对象必须通过指针访问，不能按值放进容器。
    add(std::make_unique<Sphere>(sphere));
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
 * 作用：求某个像素命中的物体，即沿射线问遍所有物体、取最近的命中者。
 * 参数：px —— 列号，取值 0 ~ width() - 1
 *       py —— 行号，取值 0 ~ height() - 1
 * 返回：指向最近命中物体的指针；没有任何物体被命中时返回 nullptr
 */
const Object* Raytracer::tracePixel(int px, int py) const
{
    // 步骤 1：像素 P 在世界坐标里的位置，射线起点是摄像机 O(0,0,0)、
    //         方向 d = P - O = P。
    const Ray ray(Vector(0.0, 0.0, 0.0), pixelToPlane(px, py));

    // 步骤 2：逐个物体求交，留下 t 最小（离摄像机最近）的那个。
    //         这里是整个渲染里唯一的多态调用点：object->intersect() 具体执行哪段
    //         代码由物体的实际类型决定。以后加新形状，这个循环一个字都不用改。
    const Object* nearest = nullptr;
    double nearestT = 0.0;

    for (const std::unique_ptr<Object>& object : objects_) {
        const std::optional<double> t = object->intersect(ray);
        if (!t.has_value()) {
            continue;
        }
        if (nearest == nullptr || *t < nearestT) {
            nearest = object.get();
            nearestT = *t;
        }
    }

    return nearest;
}

/**
 * 作用：渲染整个场景。对每个像素打一条射线，命中就涂物体色、否则涂背景色。
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
            const Object* hit = tracePixel(px, py);
            if (hit != nullptr) {
                // 只有命中的像素才需要写一次，空白区域保持构造时的背景色。
                // color() 返回的是物体的 3 元素颜色数组，正好是 setPixel 要的形状。
                bitmap.setPixel(px, py, hit->color());
            }
        }
    }

    return bitmap;
}
