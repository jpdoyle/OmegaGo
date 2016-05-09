#include "Sgf.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cctype>

void clearSpace(std::function<char (bool)> nextCh) {
    char c;
    while((c = nextCh(false)) != EOF && std::isspace(c)) {
        nextCh(true);
    }
}

ptr<SgfNode> parseSgfNode(stream nextCh) {
    /* std::cerr << "\nparseSgfNode\n"; */
    if(nextCh(false) != ';') {
        return ptr<SgfNode>();
    }

    ptr<SgfNode> ret(new SgfNode);

    nextCh(true);
    // collect all properties
    while(1) {
        clearSpace(nextCh);
        char c = nextCh(false);
        if(c < 'A' || c > 'Z') {
            break;
        }
        std::string name = "";
        while((c = nextCh(false)) != EOF) {
            if(c < 'A' || c > 'Z') {
                break;
            }
            name.push_back(nextCh(true));
        }
        /* std::cerr << "\nName: " << name << "\n"; */

        clearSpace(nextCh);
        c = nextCh(false);
        std::vector<std::string> values;
        while(c == '[') {
            /* std::cerr << "\nValue\n"; */
            nextCh(true);
            std::string val = "";
            while((c = nextCh(false)) != EOF) {
                if(c == ']') {
                    break;
                }
                val.push_back(nextCh(true));
            }
            /* std::cerr << "\n" << val << "\n"; */
            if(c == ']') {
                nextCh(true);
                values.push_back(val);
            } else {
                break;
            }

            clearSpace(nextCh);
            c = nextCh(false);
        }

        if(values.empty()) {
            return ptr<SgfNode>();
        }
        ret->propNames.push_back(name);
        ret->properties.insert({name,values});
    }
    if(ret->propNames.empty()) {
        return ptr<SgfNode>();
    }
    return ret;
}

ptr<SgfGameTree> parseSgfTree(stream nextCh) {
    /* std::cerr << "\nparseSgfTree\n"; */
    ptr<SgfGameTree> ret(new SgfGameTree);
    SgfGameTree& tree = *ret;
    if(nextCh(false) != '(') {
        ret.release();
        return ret;
    }

    nextCh(true);
    ptr<SgfNode> n;
    clearSpace(nextCh);
    while((n = parseSgfNode(nextCh))) {
        tree.nodes.push_back(std::move(*n));
        auto del = n.get_deleter();
        del(n.release());
        clearSpace(nextCh);
    }
    if(nextCh(false) != ')') {
        return ptr<SgfGameTree>();
    }
    nextCh(true);
    clearSpace(nextCh);
    auto next = parseSgfTree(nextCh);
    while(next) {
        tree.subtrees.push_back(std::move(next));
        clearSpace(nextCh);
        next = parseSgfTree(nextCh);
    }
    return ret;
}

ptr<SgfFile> parseSgfFile(std::function<char (bool)> nextCh) {
    /* std::cerr << "\nparseSgfFile\n"; */
    ptr<SgfFile> ret(new SgfFile);

    clearSpace(nextCh);

    ptr<SgfGameTree> newTree = parseSgfTree(nextCh);

    while(newTree) {
        ret->games.emplace_back(std::move(newTree));
        clearSpace(nextCh);
        newTree = parseSgfTree(nextCh);
    }

    if(ret->games.empty()) {
        ret.release();
    }
    return ret;
}

