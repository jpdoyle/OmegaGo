#include "GameServer.hpp"
#include "Board.hpp"
#include "Sgf.hpp"
#include <fstream>

void gameServer(stream getCh,std::ostream& out) {
    Board b;
    auto nextNode = [&getCh]() {
        char c;
        while((c = getCh(false)) != ';' && c != EOF) {
            getCh(true);
        }
        return parseSgfNode(getCh);
    };
    std::vector<ptr<SgfNode>> moves;
    std::map<std::string,ptr<std::ofstream>> outfiles;

    struct Cleanup {
        std::map<std::string,ptr<std::ofstream>>& outfiles_;

        Cleanup(std::map<std::string,ptr<std::ofstream>>& outfiles) :
            outfiles_(outfiles) {}
        ~Cleanup() {
            for(auto& of: outfiles_) {
                *of.second << ")\n";
                of.second->flush();
            }
        }
    } cleanup(outfiles);

    auto addFile = [&moves,&outfiles](std::string name) {
        if(outfiles.find(name) != outfiles.end()) {
            return;
        }
        outfiles.emplace(std::make_pair(name,
            ptr<std::ofstream>(
                new std::ofstream(name.c_str(),
                    std::ios_base::out|std::ios_base::ate))));
        auto& of = *outfiles.at(name);
        of << "(";
        for(const auto& n: moves) {
            of << *n << '\n';
        }
        of.flush();
    };

    auto addMove = [&moves,&outfiles](ptr<SgfNode> move) {
        auto x = *move;
        for(auto& of: outfiles) {
            *of.second << *move << '\n';
            of.second->flush();
        }
        moves.emplace_back(std::move(move));
    };

    bool done = false;
    bool passed = false;
    for(ptr<SgfNode> n = nextNode();
            getCh(false) != EOF && getCh(false) != ')' && !done;
            n = nextNode()) {
        if(n) {
            if(b.playSgfMove(*n)) {
                done = (b.lastMove.pass && passed);
                passed = b.lastMove.pass;
                if(done) {
                    out << "=\n";
                } else {
                    out << "*\n";
                }
                out << b << "\n@";
                if(done) {
                    auto scores = b.score();
                    out << "\n" << scores.first << "," << scores.second;
                }
            } else {
                out << "!";
            }
        } else {
            out << "?";
        }
        out << std::endl;
        // Do this last so that out flushes first
        if(n) {
            if(n->properties.find("P") != n->properties.end()) {
                out << b << "\n@\n";
                out.flush();
            }
            if(n->properties.find("OF") != n->properties.end()) {
                if(!n->properties.at("OF").empty()) {
                    addFile(n->properties.at("OF")[0]);
                }
            }
            n->properties.erase("P");
            n->properties.erase("OF");
            auto& names = n->propNames;
            names.erase(
                std::remove_if(names.begin(),names.end(),
                    [](std::string s) {
                        return s == "P" || s == "OF";
                    }),
                names.end());
            addMove(std::move(n));
        }
    }
}


