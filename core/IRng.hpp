#pragma once

#include <random>

namespace tracker {

class IRng {
public:
    virtual ~IRng() = default;
    
    virtual double uniform(double min = 0.0, double max = 1.0) = 0;
    virtual int uniformInt(int min, int max) = 0;
    virtual double normal(double mean = 0.0, double stddev = 1.0) = 0;
};

class StandardRng : public IRng {
private:
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    
public:
    StandardRng() : gen_(rd_()) {}
    
    double uniform(double min = 0.0, double max = 1.0) override {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(gen_);
    }
    
    int uniformInt(int min, int max) override {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(gen_);
    }
    
    double normal(double mean = 0.0, double stddev = 1.0) override {
        std::normal_distribution<double> dist(mean, stddev);
        return dist(gen_);
    }
};

} // namespace tracker