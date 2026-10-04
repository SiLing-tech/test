// ============================================================================
// settings.h —— 把所有可以调的参数集中到一处
//
// 目的：渲染代码里不再出现「魔法常量」。像素多少、摄像机多远、背景什么颜色、
//       场景里有几个球、每个球多大什么颜色放在哪里、结果写到哪里 —— 全部在这里。
//       想换效果只改一个文件，其余代码一个字都不用动。
//
// 三个类型的分工（都是「只收参数」，不负责建场景）：
//   Settings::SphereSettings —— 一个球的参数：球心、半径、颜色
//   Settings::ImageSettings  —— 画面与摄像机的参数：宽、高、距离 D、背景色
//   Settings::Settings       —— 把上面两者加上输出路径，组成完整的一套配置
//
// 依赖关系：本文件只包含 <cstdint> / <string> / <vector> 等标准库，
// 刻意不包含 geometry.h / object.h / raytracer.h。所以它是整条依赖链的上游，
// 不需要认识 Vector、Sphere、Raytracer 里的任何一个。
// 由调用方（main.cpp）负责把这里读出来的数值交给那些类型去构造，例如：
//     Settings s = Settings::makeDefault();
//     s.validate();                       // 一次性校验所有参数
//     Sphere ball(Vector(x, y, z), r, c); // 数值从这里取，类型在那里建
// 这样加新形状时本文件不用改；而如果让 Settings 自己返回 Sphere，
// 它就得认识 Sphere，上游反而依赖下游了。
//
// 用法示例（改参数就改下面 makeDefault() 里的数值）：
//     Settings s = Settings::makeDefault();
//     s.image().distance();      // 读摄像机距离 D
//     s.sphereCount();           // 场景里有几个球
//     s.sphere(i).radius();      // 第 i 个球的半径
//     s.sphere(i).volume();      // 第 i 个球的体积（派生量，见下）
// ============================================================================

#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Settings {

// ============================================================================
// 结构体 SphereSettings —— 一个球体的全部可调参数
//
//   注意只存「半径」，不存体积：光线投射的正经计算只用到半径，
//   体积是半径的派生量，用 volume() 按 4/3*pi*R^3 现算即可。
//   两者都存反而麻烦 —— 万一改了一个忘了另一个，数据就自相矛盾了。
// ============================================================================
struct SphereSettings {
    // 作用：构造一个球的参数，并立刻校验半径合法。
    // 参数：cx, cy, cz —— 球心坐标
    //       radius     —— 半径，必须 > 0
    //       rgb        —— 颜色 {红, 绿, 蓝}，每个分量取值 0~255
    // 返回：无
    // 异常：radius <= 0（含 NaN）时抛 std::invalid_argument。
    //       校验放在构造里，意味着只要这个对象存在，半径就一定合法，
    //       后面读出来用的时候不必再检查一遍。
    SphereSettings(double cx, double cy, double cz, double radius, const uint8_t (&rgb)[3]);

    // 作用：读取球心坐标。
    // 参数：无
    // 返回：对应分量
    double cx() const { return cx_; }
    double cy() const { return cy_; }
    double cz() const { return cz_; }

    // 作用：读取半径。
    // 参数：无
    // 返回：半径，恒为正数
    double radius() const { return radius_; }

    // 作用：读取颜色。
    // 参数：无
    // 返回：3 元素数组 {红, 绿, 蓝} 的引用
    // 说明：返回数组引用而不是指针，是为了保持「3 元素数组」这个类型不被退化掉，
    //       可以直接交给 Bitmap::setPixel()（它要的正是 const uint8_t (&)[3]）。
    const uint8_t (&color() const)[3] { return color_; }

    // 作用：按半径算出球的体积。
    // 参数：无
    // 返回：体积 4/3 * pi * R^3；只用于展示/打印，不参与渲染计算
    double volume() const;

private:
    double  cx_;        // 球心 x
    double  cy_;        // 球心 y
    double  cz_;        // 球心 z
    double  radius_;    // 半径（体积由它派生）
    uint8_t color_[3];  // 颜色 {红, 绿, 蓝}
};

// ============================================================================
// 结构体 ImageSettings —— 画面与摄像机的全部可调参数
//
//   宽、高同时也是投影平面上的像素数：摄像机固定在原点，投影平面在 z = D 处，
//   尺寸就是这里给的一格一像素的矩形。
// ============================================================================
struct ImageSettings {
    // 作用：构造画面与摄像机的参数，并立刻校验它们合法。
    // 参数：width, height —— 位图/投影平面的尺寸（像素），必须为正整数
    //       distance      —— 摄像机到投影平面的直线距离 D，必须 > 0
    //       background    —— 背景色 {红, 绿, 蓝}
    // 返回：无
    // 异常：宽高不是正整数、或 distance 不是正数（含 NaN）时抛 std::invalid_argument
    ImageSettings(int width, int height, double distance, const uint8_t (&background)[3]);

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

private:
    int     width_;         // 宽（像素）
    int     height_;        // 高（像素）
    double  distance_;      // 摄像机到投影平面的距离 D
    uint8_t background_[3]; // 背景色
};

// ============================================================================
// 结构体 Settings —— 一整套配置：画面 + 摄像机 + 场景里的所有球 + 输出路径
//
//   球用 std::vector 存，所以想放几个放几个，不再限于「一个球」。
//   加球用 addSphere()，例如：
//       s.addSphere(SphereSettings(0, 0, 800, 150, orange));
// ============================================================================
struct Settings {
    // 作用：构造一整套配置。
    // 参数：image      —— 画面与摄像机参数（按值传入，内部拷贝一份保存）
    //       outputPath —— 输出位图的路径
    //       spheres    —— 初始的球列表，默认为空
    // 返回：无
    Settings(const ImageSettings& image, const std::string& outputPath,
             const std::vector<SphereSettings>& spheres = {});

    // 作用：往场景里再加一个球。
    // 参数：sphere —— 球的参数
    // 返回：无
    void addSphere(const SphereSettings& sphere);

    // 作用：读取画面与摄像机参数。
    // 参数：无
    // 返回：ImageSettings 的常量引用
    const ImageSettings& image() const { return image_; }

    // 作用：读取输出路径。
    // 参数：无
    // 返回：输出位图的路径
    const std::string& outputPath() const { return outputPath_; }

    // 作用：场景里的球数。
    // 参数：无
    // 返回：已配置的球的数量
    std::size_t sphereCount() const { return spheres_.size(); }

    // 作用：读取第 index 个球的参数。
    // 参数：index —— 下标，取值 0 ~ sphereCount() - 1
    // 返回：SphereSettings 的常量引用
    // 异常：index 越界时抛 std::out_of_range
    const SphereSettings& sphere(std::size_t index) const;

    // 作用：在真正开始建场景/渲染之前，一次性检查整套配置是否可用。
    // 参数：无
    // 返回：全部通过则无返回值
    // 异常：输出路径为空、或场景里一个物体都没有时抛 std::invalid_argument
    // 说明：宽高、距离 D、每个球的半径在各自构造时就已经校验过了，
    //       所以这里只剩「整套配置是否说得通」这层检查。
    //       好处是错误在动手渲染之前就抛出来，而不是渲染到一半才失败。
    void validate() const;

    // 作用：造一套带默认值的配置，想调效果直接改这个函数里的数值即可。
    // 参数：无
    // 返回：一整套填好默认值的 Settings
    // 说明：这是全项目唯一一处写死场景数值的地方。
    static Settings makeDefault();

private:
    ImageSettings              image_;      // 画面与摄像机参数
    std::string                outputPath_; // 输出位图路径
    std::vector<SphereSettings> spheres_;   // 场景里的所有球
};

// ============================================================================
// 参数校验辅助函数
//
//   放在 Settings 命名空间这一层，而不是做成某个结构体的私有静态成员：
//   SphereSettings、ImageSettings、Settings 三者是并列关系，谁也无权访问
//   另一个的私有成员。校验规则是它们共用的工具，放在共同的命名空间里最自然。
//   两个重载分别对应「正数」与「正整数」，报错信息也更准确。
// ============================================================================

// 作用：校验一个尺寸/距离参数是正数。
// 参数：what  —— 出错提示里用的中文名称，如 "球半径"
//       value —— 待校验的值
// 返回：value 本身，便于直接写在成员初始化列表里
// 异常：value 不是正数（含 NaN）时抛 std::invalid_argument
double checkPositive(const char* what, double value);

// 作用：校验一个像素数/尺寸是正整数（宽、高这类）。
// 参数：what  —— 出错提示里用的中文名称，如 "宽"
//       value —— 待校验的值
// 返回：value 本身，便于直接写在成员初始化列表里
// 异常：value <= 0 时抛 std::invalid_argument
// 说明：单独一个 int 重载，是为了让报错信息说「必须为正整数」而不是「必须为正数」，
//       同时避免把负数转成 std::size_t（那会变成一个极大的正数，检查就失效了）。
int checkPositive(const char* what, int value);

} // namespace Settings

#endif // SETTINGS_H
