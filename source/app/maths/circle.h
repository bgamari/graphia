#ifndef CIRCLE
#define CIRCLE

#include <QRectF>
#include <cmath>

class Circle
{
public:
    Circle() = default;

    Circle(float x, float y, float radius) noexcept :
        _x(x),
        _y(y),
        _radius(radius)
    {}

    float x() const { return _x; }
    float y() const { return _y; }
    float radius() const { return _radius; }

    QPointF centre() const { return {_x, _y}; }

    QRectF boundingBox() const
    {
        return {_x - _radius, _y - _radius, _radius * 2.0f, _radius * 2.0f};
    }

    void set(float x, float y, float radius)
    {
        _x = x;
        _y = y;
        _radius = radius;
    }

    void setX(float x) { _x = x; }
    void setY(float y) { _y = y; }
    void setRadius(float radius) { _radius = radius; }

    void translate(const QPointF& translation)
    {
        _x += static_cast<float>(translation.x());
        _y += static_cast<float>(translation.y());
    }

    // Scales around origin
    void scale(float f)
    {
        _x *= f;
        _y *= f;
        _radius *= f;
    }

    float distanceToCentreSq(const Circle& other) const
    {
        float dx = other._x - _x;
        float dy = other._y - _y;

        return dx * dx + dy * dy;
    }

    float distanceToSq(const Circle& other) const
    {
        float radii = _radius + other._radius;
        float radiiSq = radii * radii;

        return distanceToCentreSq(other) - radiiSq;
    }

    float distanceTo(const Circle& other) const
    {
        return std::sqrt(distanceToCentreSq(other)) - (_radius + other._radius);
    }

    bool intersects(const Circle& other) const
    {
        float radii = _radius + other._radius;
        float radiiSq = radii * radii;

        return radiiSq > distanceToCentreSq(other);
    }

private:
    float _x = 0.0f;
    float _y = 0.0f;
    float _radius = 1.0f;
};

#endif // CIRCLE

