// ============================================================================
// bitmap.cpp —— Bitmap 类的实现（bitmap.h 中声明的成员函数）
//
// 只使用 C++ 标准库：<fstream> 写文件、<vector> 存像素、<algorithm> 复制行。
// 下面两个结构体描述的是 BMP 文件在磁盘上的真实字节布局，属于本文件的内部
// 细节，因此放在匿名 namespace 里，外部（包括 main.cpp）看不到也用不到。
//
// 内部存储约定（整个文件都依赖它）：
//   像素缓冲区 pixels_ 直接按 BMP 原生顺序存放 —— 自下而上、每像素 BGR、
//   每行补齐到 4 字节。这样 save() 可以整块写出，不用做任何转换；
//   代价是对外坐标要翻转一次，这件事只在 setPixel() 里发生。
// ============================================================================

#include "bitmap.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

#pragma pack(push, 1)
// BMP 文件头：说明「这是什么文件、总共多大、像素数据从第几字节开始」，固定 14 字节。
// 字段一律用固定宽度的整数类型，避免不同平台上 int 的长度不同而写坏文件。
struct BmpFileHeader {
    uint16_t bfType;      // 文件类型标记，必须是 'BM'（0x4D42，小端存储）
    uint32_t bfSize;      // 整个文件的字节数 = 14 + 40 + 像素数据字节数
    uint16_t bfReserved1; // 保留字段，必须为 0
    uint16_t bfReserved2; // 保留字段，必须为 0
    uint32_t bfOffBits;   // 像素数据距文件开头的偏移，本程序恒为 54
};

// BMP 信息头（BITMAPINFOHEADER）：说明「图多宽多高、每像素多少位、是否压缩」，固定 40 字节。
struct BmpInfoHeader {
    uint32_t biSize;          // 本结构体自身的大小，固定填 40
    int32_t  biWidth;         // 宽（像素）
    int32_t  biHeight;        // 高（像素）；正数表示像素自下而上存储
    uint16_t biPlanes;        // 平面数，固定为 1
    uint16_t biBitCount;      // 每像素占多少位，24 表示红绿蓝各 8 位
    uint32_t biCompression;   // 压缩方式，0 = BI_RGB（不压缩）
    uint32_t biSizeImage;     // 像素数据总字节数（含每行末尾的对齐补 0）
    int32_t  biXPelsPerMeter; // 水平分辨率，单位「像素/米」，2835 ≈ 72 DPI
    int32_t  biYPelsPerMeter; // 垂直分辨率，单位同上
    uint32_t biClrUsed;       // 调色板颜色数，24 位真彩色填 0
    uint32_t biClrImportant;  // 重要颜色数，填 0 表示全部重要
};
#pragma pack(pop)

// #pragma pack(1) 关闭了编译器的对齐填充，这两个结构体才能正好是 14 和 40 字节。
// 用 static_assert 在编译期卡住：万一将来有人改动字段，编译会直接失败，
// 而不是生成一个能编译、却打不开的坏文件。
static_assert(sizeof(BmpFileHeader) == 14, "BMP 文件头必须是 14 字节");
static_assert(sizeof(BmpInfoHeader) == 40, "BMP 信息头必须是 40 字节");

const int kFileHeaderSize = 14; // BMP 文件头的字节数
const int kInfoHeaderSize = 40; // BMP 信息头的字节数
const int kBytesPerPixel  = 3;  // 24 位色 = 每像素 3 字节

/**
 * 作用：校验构造参数是正整数，把「宽高必须为正」这条约束挡在对象建立之前。
 *       这一步不能省：成员按声明顺序初始化，如果先按非法的宽高去算 stride、
 *       分配 pixels_，就会先出现巨大的内存申请或未定义行为，根本轮不到检查。
 * 参数：value —— 待校验的值（传进来的宽或高）
 *       what  —— 出错提示里用的中文名称，如 "宽"、"高"
 * 返回：value 本身，校验通过时原样返回，方便直接写在成员初始化列表里
 * 异常：value <= 0 时抛 std::invalid_argument
 */
int checkPositive(int value, const char* what)
{
    if (value <= 0) {
        throw std::invalid_argument(std::string("Bitmap: ") + what + " 必须为正整数");
    }
    return value;
}

} // namespace

/**
 * 作用：构造一张纯色位图，并在内存中准备好整张图的像素数据。
 *       具体做四件事：校验宽高 → 算出每行字节数 → 分配并清零像素缓冲区
 *       → 按 BMP 规定的 BGR 顺序填入颜色。
 * 参数：width  —— 宽，水平方向的像素数，必须 > 0
 *       height —— 高，竖直方向的像素数，必须 > 0
 *       rgb    —— 颜色数组 {红, 绿, 蓝}，每个分量取值 0~255
 * 返回：无
 * 异常：width 或 height 不是正整数时抛 std::invalid_argument
 */
Bitmap::Bitmap(int width, int height, const uint8_t (&rgb)[3])
    : width_(checkPositive(width, "宽")),
      height_(checkPositive(height, "高")),
      stride_(rowStride(width_)) // 依赖前面的 width_，所以必须排在它后面
{
    // 步骤 1：分配像素缓冲区并全部置 0。
    //         置 0 同时解决了行末的对齐补位，这些位置必须保持为 0。
    pixels_.assign(static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_), 0);

    // 步骤 2：先拼出第一行的像素内容。
    //         注意 BMP 里每个像素按 B、G、R 存放，与 rgb 数组的 {R, G, B} 顺序相反。
    std::vector<uint8_t> row(static_cast<std::size_t>(stride_), 0);
    for (int x = 0; x < width_; ++x) {
        row[static_cast<std::size_t>(x) * kBytesPerPixel + 0] = rgb[2]; // 蓝
        row[static_cast<std::size_t>(x) * kBytesPerPixel + 1] = rgb[1]; // 绿
        row[static_cast<std::size_t>(x) * kBytesPerPixel + 2] = rgb[0]; // 红
    }

    // 步骤 3：整张图颜色相同，把这一行原样复制到每一行即可。
    //         按行复制比逐像素赋值快得多，std::copy 对连续内存会自动优化成整块搬运。
    //         各行颜色一致，所以「自下而上」的存放顺序在这一步没有影响。
    for (int y = 0; y < height_; ++y) {
        std::copy(row.begin(), row.end(),
                  pixels_.begin() + static_cast<std::size_t>(y) * stride_);
    }
}

/**
 * 作用：计算 BMP 规定的一行像素占用的字节数。
 *       BMP 要求每行从 4 字节的边界开始，所以 width * 3 之后要向上补齐：
 *       width = 5 时 5 * 3 = 15，补齐到 16（末尾多出 1 个必须为 0 的字节）。
 * 参数：width —— 宽，单位像素；调用前已保证为正数
 * 返回：一行的字节数（含行末用于对齐的补 0 字节）
 */
int Bitmap::rowStride(int width)
{
    return (width * kBytesPerPixel + 3) / 4 * 4;
}

/**
 * 作用：把坐标 (x, y) 处的那个像素改成 rgb 指定的颜色。
 *       坐标以左上角为原点：x 是列号向右增大，y 是行号向下增大。
 * 参数：x   —— 列号，取值 0 ~ width() - 1
 *       y   —— 行号，取值 0 ~ height() - 1
 *       rgb —— 新颜色 {红, 绿, 蓝}，每个分量取值 0~255
 * 返回：无
 * 异常：x 或 y 越界时抛 std::out_of_range，缓冲区不会被写坏
 */
void Bitmap::setPixel(int x, int y, const uint8_t (&rgb)[3])
{
    // 步骤 1：先做范围检查，保证后面的下标计算一定落在 pixels_ 之内
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        throw std::out_of_range("Bitmap::setPixel: 坐标越界");
    }

    // 步骤 2：定位到该像素在缓冲区里的首字节。
    //         关键的一步是行号翻转：对外约定 (0, 0) 在左上角，
    //         而内部按 BMP 原生顺序自下而上存放 —— 最上面一行 y = 0
    //         实际对应缓冲区里的最后一行 height_ - 1。
    const std::size_t row = static_cast<std::size_t>(height_ - 1 - y);
    const std::size_t offset = row * static_cast<std::size_t>(stride_) +
                               static_cast<std::size_t>(x) * kBytesPerPixel;

    // 步骤 3：写入颜色。BMP 里每像素按 B、G、R 存放，与 {R, G, B} 顺序相反。
    pixels_[offset + 0] = rgb[2]; // 蓝
    pixels_[offset + 1] = rgb[1]; // 绿
    pixels_[offset + 2] = rgb[0]; // 红
}

/**
 * 作用：把内存中的位图按 BMP 格式写入磁盘文件，
 *       文件内容依次为 14 字节文件头 + 40 字节信息头 + 全部像素数据。
 * 参数：filePath —— 输出文件路径（建议以 .bmp 结尾）；
 *                   文件已存在则覆盖，不存在则新建
 * 返回：成功且数据已完整落盘返回 true；无法创建文件或写入失败返回 false
 * 异常：不抛异常，失败一律通过返回值报告，调用方判断 if (!save(...)) 即可
 */
bool Bitmap::save(const std::string& filePath) const
{
    const uint32_t pixelBytes = static_cast<uint32_t>(pixels_.size());

    // 步骤 1：准备文件头。告诉接收方这是 BMP、整个文件多大、像素数据从第 54 字节开始。
    BmpFileHeader fileHeader{};
    fileHeader.bfType    = 0x4D42; // 小端存储下即字符 'B'、'M'
    fileHeader.bfSize    = static_cast<uint32_t>(kFileHeaderSize + kInfoHeaderSize) + pixelBytes;
    fileHeader.bfOffBits = static_cast<uint32_t>(kFileHeaderSize + kInfoHeaderSize);

    // 步骤 2：准备信息头。告诉接收方图的宽高、每像素 24 位、不压缩。
    BmpInfoHeader infoHeader{};
    infoHeader.biSize          = static_cast<uint32_t>(kInfoHeaderSize);
    infoHeader.biWidth         = width_;
    infoHeader.biHeight        = height_; // 正数：像素自下而上存储
    infoHeader.biPlanes        = 1;
    infoHeader.biBitCount      = 24;
    infoHeader.biCompression   = 0; // BI_RGB，不压缩
    infoHeader.biSizeImage     = pixelBytes;
    infoHeader.biXPelsPerMeter = 2835;
    infoHeader.biYPelsPerMeter = 2835;

    // 步骤 3：以二进制模式打开文件（trunc 表示内容重写），打不开就直接返回失败
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    // 步骤 4：依次写出文件头、信息头，最后把像素数据整块写出。
    //         内存里的排列已经是 BMP 原生顺序（自下而上、BGR、行末补 0），
    //         所以这里不需要任何转换，一次 write 即可。
    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    out.write(reinterpret_cast<const char*>(pixels_.data()),
              static_cast<std::streamsize>(pixels_.size()));

    // 步骤 5：flush 之后流状态才能反映真正的写入失败（例如磁盘写满）。
    //         若省掉这一步，返回 true 时数据可能还停在用户态缓冲区里。
    out.flush();

    return static_cast<bool>(out);
}
