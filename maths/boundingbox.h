#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include <QVector2D>
#include <QVector3D>

class BoundingBox2D
{
private:
    QVector2D _min;
    QVector2D _max;

public:
    BoundingBox2D();
    BoundingBox2D(const QVector2D& min, const QVector2D& max);

    const QVector2D& min() const { return _min; }
    const QVector2D& max() const { return _max; }

    float xLength() const { return _max.x() - _min.x(); }
    float yLength() const { return _max.y() - _min.y(); }
    float maxLength() const;

    QVector2D xVector() const { return QVector2D(xLength(), 0.0f); }
    QVector2D yVector() const { return QVector2D(0.0f, yLength()); }

    float area() const { return xLength() * yLength(); }

    void set(const QVector2D& min, const QVector2D& max);
    void expandToInclude(const QVector2D& point);
    void expandToInclude(const BoundingBox2D& other);

    bool containsPoint(const QVector2D& point) const;
    bool containsLine(const QVector2D& pointA, const QVector2D& pointB) const;

    QVector2D centre() const;

    BoundingBox2D operator+(const QVector2D v) const { return BoundingBox2D(_min + v, _max + v); }
};

class Ray;

class BoundingBox3D
{
private:
    QVector3D _min;
    QVector3D _max;

public:
    BoundingBox3D();
    BoundingBox3D(const QVector3D& min, const QVector3D& max);

    const QVector3D& min() const { return _min; }
    const QVector3D& max() const { return _max; }

    float xLength() const { return _max.x() - _min.x(); }
    float yLength() const { return _max.y() - _min.y(); }
    float zLength() const { return _max.z() - _min.z(); }
    float maxLength() const;

    QVector3D xVector() const { return QVector3D(xLength(), 0.0f, 0.0f); }
    QVector3D yVector() const { return QVector3D(0.0f, yLength(), 0.0f); }
    QVector3D zVector() const { return QVector3D(0.0f, 0.0f, zLength()); }

    void scale(float s);
    BoundingBox3D scaled(float s) const;

    float volume() const { return xLength() * yLength() * zLength(); }

    void set(const QVector3D& min, const QVector3D& max);
    void expandToInclude(const QVector3D& point);
    void expandToInclude(const BoundingBox3D& other);

    bool containsPoint(const QVector3D& point) const;
    bool containsLine(const QVector3D& pointA, const QVector3D& pointB) const;
    bool intersects(const Ray& ray, float t0, float t1) const;
    bool intersects(const Ray& ray) const;

    QVector3D centre() const;

    BoundingBox3D operator+(const QVector3D v) const { return BoundingBox3D(_min + v, _max + v); }
};

#endif // BOUNDINGBOX_H
