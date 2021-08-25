#pragma once

#include <array>
#include "policy.h"
#include "types.h"

namespace fdb {

template <class T, Quint N>
struct Dense {
    std::array<T, N * N * N> m_data;

    Dense() = default;
    ~Dense() = default;
    Dense(Dense const &) = default;
    Dense &operator=(Dense const &) = default;
    Dense(Dense &&) = default;
    Dense &operator=(Dense &&) = default;

    [[nodiscard]] static Quint linearize(Quint3 coor) {
        return dot((coor + N) % N, Quint3(1, N, N * N));
    }

    [[nodiscard]] static Quint3 delinearize(Quint i) {
        return Quint3(i % N, (i / N) % N, (i / N) / N);
    }

    [[nodiscard]] T &at(Quint3 coor) {
        Quint i = linearize(coor);
        return m_data[i];
    }

    [[nodiscard]] T const &at(Quint3 coor) const {
        Quint i = linearize(coor);
        return m_data[i];
    }

    [[nodiscard]] decltype(auto) operator()(Quint3 coor) {
        return at(coor);
    }

    [[nodiscard]] decltype(auto) operator()(Quint3 coor) const {
        return at(coor);
    }

    template <class Pol, class F>
    void foreach(Pol const &pol, F const &func) {
        range_for(pol, (Quint)0, N * N * N, [&] (Quint i) {
            Quint3 coor = delinearize(i);
            func(coor, m_data[i]);
        });
    }
};

}