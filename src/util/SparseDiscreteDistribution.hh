// SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include <random>

template<class T, class Probability>
struct SparseDiscreteDistribution {
    std::vector<std::pair<Probability, T>> values;
    std::discrete_distribution<size_t> distr;

    void update() {
        std::vector<Probability> weights;
        for (auto &pair : values) {weights.push_back(pair.first);}
        distr = std::discrete_distribution<size_t>(weights.begin(), weights.end());
    }
    template<class Generator>
    T operator()(Generator &gen) {return values.at(distr(gen)).second;}
};
