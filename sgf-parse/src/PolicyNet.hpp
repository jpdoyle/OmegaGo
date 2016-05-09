#ifndef POLICYNET_HPP
#define POLICYNET_HPP

#include <tiny_cnn/tiny_cnn.h>
#include "Util.hpp"
#include "Features.hpp"

using policy_net = tiny_cnn::network<
                    tiny_cnn::cross_entropy_multiclass,
                    tiny_cnn::gradient_descent>;

inline void make_policy_net(policy_net& nn,size_t num33,size_t filters) {
    using namespace tiny_cnn;
    using namespace tiny_cnn::activation;
    nn << convolutional_layer<relu>(19,19,5,Features::NUM_CHANNELS,
                                    filters,padding::same,false);
    for(size_t i = 0; i < num33; ++i) {
        nn << convolutional_layer<relu>(19,19,3,filters,filters,
                                        padding::same,false);
    }
    nn << convolutional_layer<softmax>(19,19,1,filters,1,
                                       padding::same,true);
}

inline void loadNN(policy_net& nn,std::string basePath,int whichOne) {


    whichOne = std::min<int>(6,whichOne);
    int numLayers = whichOne;

    if(numLayers <= 0) { // deep
        make_policy_net(nn,11,Features::NUM_CHANNELS*4);
    } else {
        make_policy_net(nn,numLayers,Features::NUM_CHANNELS*2);
    }

    basePath = basePath.substr(0,basePath.find_last_of('/')+1);
    std::stringstream ss;
    ss << basePath << "/../../nn";
    if(numLayers <= 0) {
        ss << "-deep";
    } else {
        ss << numLayers;
    }
    ss << ".txt";
    std::cerr << "NN file: " << ss.str() << std::endl;

    for(size_t i = 0; i < 5; ++i) {
        std::ifstream ifs(ss.str());
        if(ifs >> nn) {
            return;
        }
    }
}

#endif

