#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class String {
public:
    String() = default;
    String(const char* value) {
        if (value != nullptr) {
            value_ = value;
        }
    }

    String& operator=(const String&) = default;
    String& operator=(const char* value) {
        value_.clear();
        if (value != nullptr) {
            value_ = value;
        }
        return *this;
    }

    bool isEmpty() const {
        return value_.empty();
    }

    size_t length() const {
        return value_.length();
    }

    const char* c_str() const {
        return value_.c_str();
    }

private:
    std::string value_;
};
