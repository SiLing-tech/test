// ============================================================================
// geometry.cpp —— Vector 与 Ray 的实现
//
// 只使用 C++ 标准库：<cmath> 开平方。
// 这里全是纯计算，不涉及任何场景、位图或渲染的概念。
// ============================================================================

#include "geometry.h"

#include <cmath>

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
 * 返回：点积，是个标量
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
 *       direction —— 方向（允许未归一化）
 * 返回：无
 */
Ray::Ray(const Vector& origin, const Vector& direction)
    : origin_(origin), direction_(direction) {}

/**
 * 作用：求射线上参数为 t 的那个点。
 * 参数：t —— 参数值
 * 返回：origin + t * direction
 */
Vector Ray::at(double t) const
{
    return origin_ + direction_ * t;
}
