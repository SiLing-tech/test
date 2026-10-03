// ============================================================================
// geometry.h —— 光线投射用到的基础几何量：三维向量 Vector 与射线 Ray
//
// 为什么单独一个头文件：
//   下面这两样东西是「通用几何基元」，既不专属于渲染器，也不专属于某一种形状。
//   物体（object.h 里的 Sphere / 将来的 Box）需要 Ray 来表达「你和我交在哪里」，
//   渲染器（raytracer.h）需要 Ray 来表达「从摄像机射出的一条视线」。
//   如果把它们放在 raytracer.h 里，object.h 就得反过来包含 raytracer.h，
//   而 raytracer.h 又要包含 object.h，形成循环包含。
//   抽出来之后依赖关系是干净的单向链：
//       bitmap.h  →  geometry.h  →  object.h  →  raytracer.h  →  main.cpp
// ============================================================================

#ifndef GEOMETRY_H
#define GEOMETRY_H

// ============================================================================
// 类 Vector —— 三维向量（点也算向量：从原点指向该点的向量）
//
//   只用得上光线投射需要的最小操作：加减、数乘、点积、求模长。
//   刻意做成不可变对象（所有运算都返回新对象），避免不小心改到场景数据。
// ============================================================================
class Vector {
public:
    // 作用：用三个分量构造向量。
    // 参数：x, y, z —— 三个分量
    // 返回：无
    Vector(double x, double y, double z);

    // 作用：读取三个分量。
    // 参数：无
    // 返回：对应的分量值
    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }

    // 作用：两个向量相加，得到新向量。
    // 参数：other —— 被加的那个向量
    // 返回：this + other
    Vector operator+(const Vector& other) const;

    // 作用：两个向量相减，得到新向量。
    // 参数：other —— 被减的那个向量
    // 返回：this - other
    Vector operator-(const Vector& other) const;

    // 作用：把向量按比例放缩，得到新向量。
    // 参数：k —— 放缩系数
    // 返回：this * k
    Vector operator*(double k) const;

    // 作用：计算两个向量的点积（内积），也就是 |a||b|cos(夹角)。
    // 参数：other —— 另一个向量
    // 返回：点积，是个标量；两个向量垂直时为 0
    double dot(const Vector& other) const;

    // 作用：计算向量的模长（长度）。
    // 参数：无
    // 返回：|this| = sqrt(this·this)，恒为非负数
    double length() const;

private:
    double x_;  // 分量 x
    double y_;  // 分量 y
    double z_;  // 分量 z
};

// ============================================================================
// 类 Ray —— 一条射线，参数方程 P(t) = origin + t * direction
//
//   t 是参数而不是真实距离：方向向量没归一化时 |t| 不等于长度。
//   本节的射线方向就是像素的世界坐标减去原点，故意保持未归一化 ——
//   这样 t = 1 恰好对应投影平面上的那个像素，判断「交点在摄像机前方」
//   只需要看 t 的正负，不必再做一次开方。
// ============================================================================
class Ray {
public:
    // 作用：用起点和方向构造一条射线。
    // 参数：origin    —— 起点
    //       direction —— 方向（允许未归一化，但必须是零向量以外）
    // 返回：无
    // 说明：这里不校验 direction 是否为零向量：零方向会让 a = d·d = 0，
    //       求根时出现除以 0。本节的射线来自像素坐标，dz 恒等于 D > 0，
    //       不可能为零，所以把校验留在调用方能保证的地方，不在这里付代价。
    Ray(const Vector& origin, const Vector& direction);

    // 作用：读取射线的起点。
    // 参数：无
    // 返回：起点坐标
    const Vector& origin() const { return origin_; }

    // 作用：读取射线的方向。
    // 参数：无
    // 返回：方向向量
    const Vector& direction() const { return direction_; }

    // 作用：求射线上参数为 t 的那个点，即 origin + t * direction。
    // 参数：t —— 参数值，t = 0 是起点，t = 1 是起点沿方向走一个「方向向量」远
    // 返回：该点坐标
    Vector at(double t) const;

private:
    Vector origin_;     // 起点
    Vector direction_;  // 方向
};

#endif // GEOMETRY_H
