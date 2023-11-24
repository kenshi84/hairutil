#pragma once

template <typename T>
class UniformIntDistribution {
public:
    UniformIntDistribution(T a, T b) : a_(a), b_(b) {}
    template <class G>
    T operator()(G& g) const {
        using U = typename G::result_type;
        const U random_range = g.max() - g.min();
        const U range = b_ - a_;
        const U scaling = random_range / range;
        const U limit = range * scaling;
        U answer;
        do {
            answer = g();
        } while (answer >= limit);
        return  static_cast<T>(answer / scaling);
    }
private:
    T a_;
    T b_;
};
