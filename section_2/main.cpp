// ============================================================================
// main.cpp —— 演示 Bitmap 类的用法
//
// 流程：用 3 元素数组描述颜色 → 构造 Bitmap → 改一个像素 → 保存成 output.bmp。
// 编译：g++ -std=c++17 -Wall -Wextra -pedantic -O2 -o bitmap_demo.exe main.cpp bitmap.cpp
// 运行：bitmap_demo.exe，会在当前目录生成 output.bmp
// ============================================================================

#include "bitmap.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

/**
 * 作用：程序入口。创建一张 320 x 200 的橙色位图，把 (20, 30) 处的像素改成红色，
 *       保存为当前目录下的 output.bmp，然后把尺寸和颜色打印到标准输出。
 * 参数：无（不使用命令行参数；如需从命令行读宽高，可改成
 *       int main(int argc, char* argv[]) 再用 std::stoi 解析）
 * 返回：全部成功返回 0；保存失败或发生异常返回 1
 */
int main()
{
    // 颜色就是 3 元素数组：{红, 绿, 蓝}，每个分量取值 0~255
    const uint8_t orange[3] = {255, 128, 0};   // 背景色
    const uint8_t red[3]    = {255, 0, 0};     // 要改成的颜色

    try {
        // 宽 320、高 200；构造时整张图已被填成 orange。
        // 注意不能用 const：下面要调 setPixel() 修改像素。
        Bitmap bitmap(320, 200, orange);

        // 把 x = 20、y = 30 处的像素改成红色。
        // 坐标以左上角为原点，x 向右、y 向下；越界会抛 std::out_of_range。
        bitmap.setPixel(20, 30, red);

        const std::string path = "output.bmp";
        if (!bitmap.save(path)) {                 // save 用返回值报告失败，不抛异常
            std::cerr << "保存失败：" << path << "\n";
            return 1;
        }

        // 成功：打印文件名、尺寸、背景色和刚改的那个像素（uint8_t 需转成 int 再打印）
        std::cout << "已生成位图：" << path
                  << "  宽 x 高 = " << bitmap.width() << " x " << bitmap.height()
                  << "  背景 RGB(" << static_cast<int>(orange[0]) << ", "
                                   << static_cast<int>(orange[1]) << ", "
                                   << static_cast<int>(orange[2]) << ")"
                  << "  像素(20, 30) = RGB(" << static_cast<int>(red[0]) << ", "
                                             << static_cast<int>(red[1]) << ", "
                                             << static_cast<int>(red[2]) << ")\n";
    }
    catch (const std::exception& e) {
        // 构造函数可能因宽高非法抛 std::invalid_argument，
        // setPixel 可能因坐标越界抛 std::out_of_range，这里统一兜住
        std::cerr << "错误：" << e.what() << "\n";
        return 1;
    }

    return 0;
}
