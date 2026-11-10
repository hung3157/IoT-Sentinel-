#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class String {
public:
    String() = default;
    String(const char* value) : value_(value == nullptr ? "" : value) {}

    String& operator=(const String&) = default;
    String& operator=(const char* value) {
        value_ = value == nullptr ? "" : value;
        return *this;
    }

    bool isEmpty() const {
        return value_.empty();
    }

private:
    std::string value_;
};
