#ifndef ASYNCPOLICY_HPP
#define ASYNCPOLICY_HPP

#include "PolicyNet.hpp"
#include "MonteCarloTree.hpp"
#include <atomic>
#include <stack>
#include <mutex>
#include <condition_variable>
#include <thread>

class Policy {
    policy_net nn;
    std::array<Features,19*19> feats;
    std::array<Features,19*19> oriented;
    tiny_cnn::vec_t inVec;
    const tiny_cnn::vec_t* result = nullptr;

public:

    Policy(std::string basePath,int whichOne) {
        loadNN(nn,basePath,whichOne);
    }

    void eval(MonteCarloNode& n) {
        auto& b = n.board;
        auto sym = n.orientation;

        feats.fill(Features());
        oriented.fill(Features());
        extractFeatures(b,feats);
        dihedralTranspose(feats,oriented,sym);

        inVec.clear();
        convertToTCNNInput(oriented,inVec);

        result = &nn.fprop(inVec);
        std::vector<size_t> inds;
        inds.assign(n.moves.size(),0);

        double total = 0;
        for(size_t i = 0; i < n.moves.size(); ++i) {
            inds[i] = /*IX(n.moves[i]);*/IX(dihedral(n.moves[i],sym));
            total += (*result)[inds[i]];
        }

        for(size_t i = 0; i < n.moves.size(); ++i) {
            n.probabilities[i].store((*result)[inds[i]]/total);
        }

    }

};

#endif

