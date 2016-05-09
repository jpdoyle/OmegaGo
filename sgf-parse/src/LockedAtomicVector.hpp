#ifndef LOCKEDATOMICVECTOR_HPP
#define LOCKEDATOMICVECTOR_HPP

#include <atomic>
#include <sstream>
#include <stdexcept>

template<class T>
class locked_atomic_vector {
    std::atomic<T>* arr;
    size_t size_;

public:

    explicit locked_atomic_vector(size_t size) :
            arr((!size) ? nullptr : new std::atomic<T>[size]),
            size_(size) {}
    locked_atomic_vector(const locked_atomic_vector &) = delete;
    locked_atomic_vector(locked_atomic_vector&& other) {
        arr = other.arr;
        other.arr = nullptr;
        size_ = other.size_;
        other.size_ = 0;
    }

    ~locked_atomic_vector() {
        if(arr) {
            delete [] arr;
        }
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return !size_;
    }

    const std::atomic<T>* cbegin() const {
        return arr;
    }
    const std::atomic<T>* cend() const {
        return arr+size_;
    }

    const std::atomic<T>* begin() const {
        return arr;
    }
    const std::atomic<T>* end() const {
        return arr+size_;
    }

    std::atomic<T>* begin() {
        return arr;
    }
    std::atomic<T>* end() {
        return arr+size_;
    }

    std::atomic<T>& operator[](size_t i) {
        return arr[i];
    }

    std::atomic<T>& at(size_t i) {
        if(i >= size_) {
            std::stringstream ss;
            ss << "locked_atomic_vector: " << i << " >= " << size_;
            throw std::out_of_range(ss.str());
        }
        return arr[i];
    }

    const std::atomic<T>& operator[](size_t i) const {
        return arr[i];
    }

    const std::atomic<T>& at(size_t i) const {
        if(i >= size_) {
            std::stringstream ss;
            ss << "locked_atomic_vector: " << i << " >= " << size_;
            throw std::out_of_range(ss.str());
        }
        return arr[i];
    }
};

#endif

