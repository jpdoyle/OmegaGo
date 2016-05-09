#ifndef UTIL_HPP
#define UTIL_HPP

#include <istream>
#include <iostream>
#include <functional>
#include <memory>
#include <type_traits>
#include <sstream>

#ifdef NDEBUG
  #define DBOUT( x )
#else
  #define DBOUT( x )  x
#endif

template<typename T>
using ptr=std::unique_ptr<T>;

template<class Arr>
int doFind(Arr& a,int i) {
    int k = i;
    while(a[k] >= 0) {
        k = a[k];
    }
    if(i != k) {
        a[i] = k;
    }
    return k;
}

template<class Arr>
int doFind(const Arr& a,int i) {
    int k = i;
    while(a[k] >= 0) {
        k = a[k];
    }
    return k;
}

template<class Arr>
int doUnion(Arr& a,int i,int k) {
    int iclass = doFind(a,i);
    int kclass = doFind(a,k);
    if(iclass == kclass) {
        return iclass;
    }
    if(a[iclass] < a[kclass]) {
        a[kclass] = iclass;
        return iclass;
    } else if(a[iclass] == a[kclass]) {
        a[kclass] = iclass;
        a[iclass]--;
        return iclass;
    } else {
        a[iclass] = kclass;
        return kclass;
    }
}

// A stream. For a stream s, s(false) should return the next character
// in the stream, without removing it. s(true) should return the next
// character and remove it.
using stream = std::function<char (bool)>;
template<class S>
typename std::enable_if<std::is_base_of<std::istream,S>::value,
                        stream>::type
makeStream(S& s) {
    return [&s](bool next) {
        return next ? s.get() : s.peek();
    };
}

template<class F>
typename std::enable_if<
    std::is_convertible<char,
        typename std::result_of<F(bool)>::type>::value,
    stream>::type
makeStream(F f) {
    return [f](bool b) { return f(b); };
}

static inline stream makeStream(std::string s) {
    auto buf = std::make_shared<std::pair<std::string,size_t>>(s,0);
    return [buf](bool next) -> char {
        if(buf->second >= buf->first.size()) {
            return EOF;
        }
        char ret = buf->first.at(buf->second);
        if(next) {
            ++buf->second;
        }
        return ret;
    };
}

template<class F>
static inline stream debugStream(stream s,F writeCh) {
    return [s,writeCh](bool next) {
        char c = s(next);
        if(next) {
            writeCh(c);
        }
        return c;
    };
}

inline bool expect(stream s,std::string str) {
    for(auto c:str) {
        if(s(false) != c) {
            return false;
        }
        s(true);
    }
    return true;
}

inline char oneOf(stream s,std::string chars,char def='\0') {
    if(chars.find(s(false)) != chars.npos) {
        return s(true);
    }
    return def;
}

inline std::string anyOf(stream s,std::string chars) {
    std::string ret;
    while(chars.find(s(false)) != chars.npos) {
        ret.push_back(s(true));
    }
    return ret;
}

template<class I = size_t>
I readInt(stream s,I defaultVal = 0) {
    std::string str = anyOf(s,"0123456789");
    try {
        /* std::cout << "Int: (" << str << ")" << std::endl; */
        I ret = defaultVal;
        std::stringstream ss(str);
        if(ss >> ret) {
            return ret;
        }
    } catch(std::exception) {}
    return defaultVal;
}

template<class I = size_t>
std::pair<I,I> readCoord(stream s,I defaultVal = 0) {
    std::string str = anyOf(s,"<");
    I x = readInt(s,defaultVal);
    anyOf(s," ,");
    I y = readInt(s,defaultVal);
    anyOf(s,">");
    return {x,y};
}

inline void spaces(stream s) {
    while(std::isspace(s(false))) {
        s(true);
    }
}

#endif

