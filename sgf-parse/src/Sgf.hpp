#ifndef SGF_HPP
#define SGF_HPP

#include <vector>
#include <map>

#include "Util.hpp"

struct SgfNode {
    std::vector<std::string> propNames;
    std::map<std::string,std::vector<std::string>> properties;
};

struct SgfGameTree {
    std::vector<SgfNode> nodes;
    std::vector<ptr<SgfGameTree>> subtrees;
};

struct SgfFile {
    std::vector<ptr<SgfGameTree>> games;
};

ptr<SgfNode> parseSgfNode(stream);
ptr<SgfGameTree> parseSgfTree(stream);
ptr<SgfFile> parseSgfFile(stream);

template<class OS>
OS& operator<<(OS& os, const SgfNode& node) {
    os << ";";
    for(const auto& n: node.propNames) {
        os << n;
        for(const auto& x:node.properties.at(n)) {
            os << "[" << x << "]";
        }
    }
    return os;
}

template<class OS>
OS& operator<<(OS& os, const SgfGameTree& tree) {
    os << "(";
    for(const auto& n: tree.nodes) {
        os << n;
    }
    for(const auto& st: tree.subtrees) {
        os << *st;
    }
    os << ")";
    return os;
}

template<class OS>
OS& operator<<(OS& os, const SgfFile& file) {
    for(const auto& game: file.games) {
        os << *game;
    }
    return os;
}

#endif

