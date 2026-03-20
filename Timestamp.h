#pragma once

#include <string>
#include <iostream>

class Timestamp{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondssinceEpoch_);
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};