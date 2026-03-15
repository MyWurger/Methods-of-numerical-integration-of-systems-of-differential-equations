#pragma once

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <vector>

class TVector
{
public:
    TVector() = default;
    explicit TVector(std::size_t size, double value = 0.0) : data_(size, value)
    {
    }

    TVector(std::initializer_list<double> values) : data_(values)
    {
    }

    std::size_t Size() const noexcept
    {
        return data_.size();
    }

    bool Empty() const noexcept
    {
        return data_.empty();
    }

    double& operator[](std::size_t index)
    {
        return data_.at(index);
    }

    const double& operator[](std::size_t index) const
    {
        return data_.at(index);
    }

    const std::vector<double>& Data() const noexcept
    {
        return data_;
    }

    std::vector<double>& Data() noexcept
    {
        return data_;
    }

private:
    std::vector<double> data_;
};

inline void EnsureSameSize(const TVector& lhs, const TVector& rhs)
{
    if (lhs.Size() != rhs.Size())
    {
        throw std::invalid_argument("Векторы должны иметь одинаковую размерность");
    }
}

inline TVector operator+(const TVector& lhs, const TVector& rhs)
{
    EnsureSameSize(lhs, rhs);
    TVector result(lhs.Size());
    for (std::size_t i = 0; i < lhs.Size(); ++i)
    {
        result[i] = lhs[i] + rhs[i];
    }
    return result;
}

inline TVector operator-(const TVector& lhs, const TVector& rhs)
{
    EnsureSameSize(lhs, rhs);
    TVector result(lhs.Size());
    for (std::size_t i = 0; i < lhs.Size(); ++i)
    {
        result[i] = lhs[i] - rhs[i];
    }
    return result;
}

inline TVector operator*(const TVector& vector, double scalar)
{
    TVector result(vector.Size());
    for (std::size_t i = 0; i < vector.Size(); ++i)
    {
        result[i] = vector[i] * scalar;
    }
    return result;
}

inline TVector operator*(double scalar, const TVector& vector)
{
    return vector * scalar;
}

inline TVector operator/(const TVector& vector, double scalar)
{
    if (scalar == 0.0)
    {
        throw std::invalid_argument("Деление вектора на ноль недопустимо");
    }

    TVector result(vector.Size());
    for (std::size_t i = 0; i < vector.Size(); ++i)
    {
        result[i] = vector[i] / scalar;
    }
    return result;
}

