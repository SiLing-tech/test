// ============================================================================
// settings.cpp —— Settings 命名空间里三个类型的实现
//
// 只使用 C++ 标准库：<stdexcept> 抛异常、<string> 拼错误信息、<cmath> 算体积。
// 本文件不包含项目里任何其他头文件 —— 它处在依赖链的最上游，谁也不依赖。
// ============================================================================

#include "settings.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace Settings {

namespace {

// 圆周率。用 <cmath> 里的 M_PI 不是标准 C++（MSVC 上还要先定义 _USE_MATH_DEFINES），
// 所以自己写一个常量，避免依赖编译器的扩展。
const double kPi = 3.14159265358979323846;

} // namespace

// ============================================================================
// 参数校验（两个重载，分别对应「正数」和「正整数」）
// ============================================================================

/**
 * 作用：校验一个尺寸/距离参数是正数。
 * 参数：what  —— 出错提示里用的中文名称
 *       value —— 待校验的值
 * 返回：value 本身，便于直接写在成员初始化列表里
 * 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
 */
double checkPositive(const char* what, double value)
{
    // 写成 !(value > 0.0) 而不是 value <= 0.0：前者连 NaN 也一并挡住
    if (!(value > 0.0)) {
        throw std::invalid_argument(std::string("Settings: ") + what + " 必须为正数");
    }
    return value;
}

/**
 * 作用：校验一个像素数/尺寸是正整数。
 * 参数：what  —— 出错提示里用的中文名称
 *       value —— 待校验的值
 * 返回：value 本身
 * 异常：value <= 0 时抛 std::invalid_argument
 */
int checkPositive(const char* what, int value)
{
    if (value <= 0) {
        throw std::invalid_argument(std::string("Settings: ") + what + " 必须为正整数");
    }
    return value;
}

// ============================================================================
// 结构体 SphereSettings
// ============================================================================

/**
 * 作用：构造一个球的参数，并立刻校验半径合法。
 * 参数：cx, cy, cz —— 球心坐标
 *       radius     —— 半径，必须 > 0
 *       rgb        —— 颜色 {红, 绿, 蓝}
 * 返回：无
 * 异常：radius <= 0 时抛 std::invalid_argument（异常在成员初始化阶段抛出，
 *       所以非法配置根本建立不起来，后续不必再检查）
 */
SphereSettings::SphereSettings(double cx, double cy, double cz,
                               double radius, const uint8_t (&rgb)[3])
    : cx_(cx), cy_(cy), cz_(cz),
      radius_(checkPositive("球半径", radius))
{
    for (int i = 0; i < 3; ++i) {
        color_[i] = rgb[i];
    }
}

/**
 * 作用：按半径算出球的体积，即 4/3 * pi * R^3。
 * 参数：无
 * 返回：体积，恒为正数
 * 说明：这是派生量，不存成员。渲染只用半径，体积纯给展示和检查用。
 */
double SphereSettings::volume() const
{
    return 4.0 / 3.0 * kPi * radius_ * radius_ * radius_;
}

// ============================================================================
// 结构体 ImageSettings
// ============================================================================

/**
 * 作用：构造画面与摄像机的参数，并立刻校验它们合法。
 * 参数：width, height —— 位图/投影平面的尺寸（像素），必须为正整数
 *       distance      —— 摄像机到投影平面的距离 D，必须 > 0
 *       background    —— 背景色 {红, 绿, 蓝}
 * 返回：无
 * 异常：宽高为 0、或 distance 不是正数时抛 std::invalid_argument
 */
ImageSettings::ImageSettings(int width, int height, double distance,
                             const uint8_t (&background)[3])
    : width_(checkPositive("宽", width)),
      height_(checkPositive("高", height)),
      distance_(checkPositive("距离 D", distance))
{
    for (int i = 0; i < 3; ++i) {
        background_[i] = background[i];
    }
}

// ============================================================================
// 结构体 Settings
// ============================================================================

/**
 * 作用：构造一整套配置。
 * 参数：image      —— 画面与摄像机参数
 *       outputPath —— 输出位图路径
 *       spheres    —— 初始球列表
 * 返回：无
 */
Settings::Settings(const ImageSettings& image, const std::string& outputPath,
                   const std::vector<SphereSettings>& spheres)
    : image_(image), outputPath_(outputPath), spheres_(spheres)
{
}

/**
 * 作用：往场景里再加一个球。
 * 参数：sphere —— 球的参数
 * 返回：无
 */
void Settings::addSphere(const SphereSettings& sphere)
{
    spheres_.push_back(sphere);
}

/**
 * 作用：读取第 index 个球的参数。
 * 参数：index —— 下标，取值 0 ~ sphereCount() - 1
 * 返回：SphereSettings 的常量引用
 * 异常：index 越界时抛 std::out_of_range
 */
const SphereSettings& Settings::sphere(std::size_t index) const
{
    // 用 at() 而不是 operator[]：越界时抛异常而不是读越界内存，
    // 报错信息里也带上了出错的下标，便于定位。
    return spheres_.at(index);
}

/**
 * 作用：在真正开始建场景/渲染之前，一次性检查整套配置是否说得通。
 * 参数：无
 * 返回：无
 * 异常：输出路径为空、或场景里一个物体都没有时抛 std::invalid_argument
 * 说明：宽高、距离 D、每个球的半径在各自构造时已校验，这里只补「整体」层面的检查。
 */
void Settings::validate() const
{
    if (outputPath_.empty()) {
        throw std::invalid_argument("Settings: 输出路径不能为空");
    }
    if (spheres_.empty()) {
        throw std::invalid_argument(
            "Settings: 场景里一个物体都没有，渲染出来会是一整张背景色");
    }
}

/**
 * 作用：造一套带默认值的配置。全项目唯一写死场景数值的地方，想换效果改这里。
 * 参数：无
 * 返回：填好默认值的 Settings
 */
Settings Settings::makeDefault()
{
    // ---------- 颜色：一律是 {红, 绿, 蓝}，每个分量 0~255 ----------
    const uint8_t black[3]  = {0, 0, 0};       // 背景
    const uint8_t orange[3] = {255, 80, 0};    // 球的颜色
    const uint8_t green[3]  = {0, 200, 60};    // 备用：第二个球的颜色
    const uint8_t blue[3]   = {40, 120, 255};  // 备用：第三个球的颜色

    // ---------- 画面与摄像机 ----------
    // 宽高就是位图尺寸，也就是投影平面上的像素数；
    // D 是摄像机（世界原点）到投影平面的直线距离。
    ImageSettings image(
        640,        // 宽（像素）
        480,        // 高（像素）
        500.0,      // 摄像机到投影平面的距离 D
        black);     // 背景色

    // ---------- 场景 ----------
    Settings s(image, "sphere.bmp");

    // 球：球心坐标、半径、颜色。
    // 球心放在 (0, 0, 800)：z 要大于 D，才在投影平面前方（球能被看到）。
    s.addSphere(SphereSettings(0.0, 0.0, 800.0, 150.0, orange));

    // 想再加一个球，把下面这行的注释去掉即可：
    // s.addSphere(SphereSettings(120.0, 0.0, 900.0, 100.0, green));

    // 想试试别的形状，可以自己写一个继承 Object 的类（如 Box），
    // 然后调用 tracer.add(std::make_unique<Box>(...))，本文件不用改。
    (void)green;
    (void)blue;

    return s;
}

} // namespace Settings
