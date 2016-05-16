#include <iostream>
#include <sstream>
#include <iterator>
#include <fstream>
#include <random>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include "Sgf.hpp"
#include "Board.hpp"
#include "Bot.hpp"
#include "GameServer.hpp"
#include "Features.hpp"
#include <basen.hpp>
#include "PolicyNet.hpp"

struct StreamRestore {
    const std::ios::fmtflags f;
    std::ostream& os;
    StreamRestore(std::ostream& o) : f(o.flags()),os(o) {}
    ~StreamRestore() {
        os.flags(f);
    }
};

void handler(int sig) {
    void *array[100];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, sizeof(array)/sizeof(void*));

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

int evalPolicyNet(std::string filename,int argc,
                   char* argv[]) {
    using namespace tiny_cnn;
    policy_net nn;
    size_t numLayers = 1,numFilters = Features::NUM_CHANNELS*2;
    if(argc >= 2 && std::string(argv[0]) == "--layers") {
        try {
            numLayers = std::stoi(argv[1]);
        } catch(std::exception e) {}
        argc -= 2;
        argv += 2;
    }
    if(argc >= 2 && std::string(argv[0]) == "--filters") {
        try {
            numFilters = std::stoi(argv[1]);
        } catch(std::exception e) {}
        argc -= 2;
        argv += 2;
    }

    std::cout << numLayers << " layers\n";
    std::cout << "Weights file: " << filename << std::endl;
    make_policy_net(nn,numLayers,numFilters);
    std::cout << "Out dim: " << nn.out_dim() << std::endl;

    {
        std::ifstream nnFile(filename.c_str());
        if(!nnFile) {
            std::cout << "No net file\n";
            return -1;
        } else {
            nnFile >> nn;
        }
    }

    std::cout << "NN has " << nn.depth() << " layers\n";

    constexpr decltype(Features().pack()) pack_obj = {};
    constexpr size_t bytesPer = pack_obj.size();
    constexpr size_t packSize = 19*19*bytesPer;

    std::vector<vec_t> train_feats;
    std::vector<label_t> train_moves;

    for(int i = 0; i < argc; ++i) {
        std::ifstream ifs(argv[i]);
        std::vector<uint8_t> packed;
        packed.reserve(packSize);
        std::string packStr;
        std::getline(ifs,packStr);
        bn::decode_b64(packStr.begin(),packStr.end(),
                   std::insert_iterator<std::vector<uint8_t>>(
                       packed,packed.end()));
        assert(packed.size() == packSize);

        std::array<Features,19*19> feats;
        auto it = packed.begin();
        for(auto& f: feats) {
            std::array<uint8_t,bytesPer> pack;
            std::copy(it,it+bytesPer,pack.begin());
            it += bytesPer;
            f = Features(pack);
        }
        train_feats.push_back(vec_t());
        convertToTCNNInput(feats,train_feats.back());

        std::getline(ifs,packStr);
        auto p = readCoord(makeStream(ifs));
//        std::cout <<"coord " << p.first <<","<<p.second
//                  << ": " <<IX(p) <<std::endl;
        train_moves.push_back(IX(p));
    }

    for(size_t i = 0; i < train_feats.size(); ++i) {
        auto output = nn.predict(train_feats[i]);
        std::cout << "\n\n\nFile " << argv[i] << ":\n\n";
        constexpr size_t precision = 4;
        constexpr size_t width = precision+1;
        for(size_t j = 0; j < width*21+1; ++j) {
            std::cout << "#";
        }
        for(size_t y = 0; y < 19; ++y) {
            std::cout << "\n";
            for(size_t j = 0; j < width; ++j) {
                std::cout << "#";
            }
            for(size_t x = 0; x < 19; ++x) {
                auto val = output[IX({x,y})];
                int cutoff = 1;
                for(size_t j = 0; j < precision; ++j) {
                    val *= 10;
                    cutoff *= 10;
                }
                cutoff /= 10;
                int percent = val + 0.5;
                std::cout << " ";
                while(cutoff > 1) {
                    if(percent < cutoff) {
                        std::cout << "0";
                    }
                    cutoff /= 10;
                }
                std::cout << percent;
            }
            std::cout << " ";
            for(size_t j = 0; j < width; ++j) {
                std::cout << "#";
            }
        }
        std::cout << "\n";
        for(size_t j = 0; j < width*21+1; ++j) {
            std::cout << "#";
        }
    }
    tiny_cnn::result res = nn.test(train_feats, train_moves);
    std::cout << res.num_success << "/" << res.num_total << std::endl;
    return 0;
}

int trainPolicyNet(bool justTest,std::string filename,int argc,
                   char* argv[]) {
    using namespace tiny_cnn;
    policy_net nn;
    size_t numLayers = 1,numFilters = Features::NUM_CHANNELS*2;
    if(argc >= 2 && std::string(argv[0]) == "--layers") {
        try {
            numLayers = std::stoi(argv[1]);
        } catch(std::exception e) {}
        argc -= 2;
        argv += 2;
    }
    if(argc >= 2 && std::string(argv[0]) == "--filters") {
        try {
            numFilters = std::stoi(argv[1]);
        } catch(std::exception e) {}
        argc -= 2;
        argv += 2;
    }
    bool runTest = true;
    if(argc >= 1 && std::string(argv[0]) == "--notest") {
        runTest = false;
        argc -= 1;
        argv += 1;
    }
    std::cout << numLayers << " layers\n";
    std::cout << "Weights file: " << filename << std::endl;
    make_policy_net(nn,numLayers,numFilters);
    std::cout << "Out dim: " << nn.out_dim() << std::endl;
    bool reset_weights = false;

    {
        std::ifstream nnFile(filename.c_str());
        if(!nnFile) {
            std::cout << "Recreating file\n";
            nnFile.close();
            reset_weights = true;
        } else {
            nnFile >> nn;
        }
    }

    std::cout << "NN has " << nn.depth() << " layers\n";

    std::vector<vec_t> train_feats;
    std::vector<label_t> train_moves;

    constexpr decltype(Features().pack()) pack_obj = {};
    constexpr size_t bytesPer = pack_obj.size();
    constexpr size_t packSize = 19*19*bytesPer;

    for(int i = 0; i < argc; ++i) {
        std::ifstream ifs(argv[i]);
        std::vector<uint8_t> packed;
        packed.reserve(packSize);
        std::string packStr;
        std::getline(ifs,packStr);
        bn::decode_b64(packStr.begin(),packStr.end(),
                   std::insert_iterator<std::vector<uint8_t>>(
                       packed,packed.end()));
        assert(packed.size() == packSize);

        std::array<Features,19*19> feats;
        auto it = packed.begin();
        for(auto& f: feats) {
            std::array<uint8_t,bytesPer> pack;
            std::copy(it,it+bytesPer,pack.begin());
            it += bytesPer;
            f = Features(pack);
        }
        train_feats.push_back(vec_t());
        convertToTCNNInput(feats,train_feats.back());

        std::getline(ifs,packStr);
        auto p = readCoord(makeStream(ifs));
//        std::cout <<"coord " << p.first <<","<<p.second
//                  << ": " <<IX(p) <<std::endl;
        train_moves.push_back(IX(p));
    }

    if(runTest) {
        tiny_cnn::result res = nn.test(train_feats, train_moves);
        std::cout << res.num_success << "/" << res.num_total << std::endl;
    }
    if(justTest) {
        return 0;
    }

    std::stringstream ss;
    ss << nn;

    std::cout << "start training" << std::endl;

    int minibatch_size = 128;
    int num_epochs = 1;

    nn.optimizer().alpha = 0.006;

    std::cout << "alpha = " << nn.optimizer().alpha
              << std::endl;

    std::cout << "reset_weights = " << reset_weights << "\n";

    tiny_cnn::progress_display disp(train_feats.size());
    timer t;

    /* size_t i = 0; */

    // create callback
    auto on_enumerate_epoch = [&](){
        std::cout << t.elapsed() << "s elapsed." << std::endl;
        std::ofstream ofs(filename.c_str(),std::ios::trunc);
        ofs << nn;

        /* if(i % 10 == 0) { */
        /*     tiny_cnn::result res = nn.test(train_feats, train_moves); */
        /*     std::cout << res.num_success << "/" << res.num_total << std::endl; */
        /* } */
        /* ++i; */

        disp.restart(train_feats.size());
        t.restart();
    };

    auto on_enumerate_minibatch = [&](){
        disp += minibatch_size;
    };
    // training
    nn.train(train_feats, train_moves, minibatch_size, num_epochs,
             on_enumerate_minibatch, on_enumerate_epoch,
             reset_weights);

    std::cout << "end training." << std::endl;

    /* { */
    /*     tiny_cnn::result res = nn.test(train_feats, train_moves); */
    /*     std::cout << res.num_success << "/" << res.num_total << std::endl; */
    /* } */

    /* nn.test(train_feats, train_moves).print_detail(std::cout); */

    std::ofstream ofs(filename.c_str(),std::ios::trunc);
    ofs << nn;

/*     std::stringstream ss2; */
/*     ss2 << nn; */
/*     if(ss.str() == ss2.str()) { */
/*         throw std::logic_error("Training didn't change anything"); */
/*     } */

    return 0;
}

int main(int argc,char* argv[]) {
    signal(SIGSEGV, handler);
    if(argc < 2) {
        std::cout << "no file given\n";
        return -1;
    } else {
        std::string filename = argv[1];
        bool parse = false;
        bool printFeatures = false;
        bool verbose = false;

        auto writeFeatures = [&](size_t moveNum,const Board& b,
                                 Board::Coord moveMade) {
            auto cleanFile = filename;
            auto slashInd = cleanFile.find_last_of('/');
            if(slashInd != std::string::npos) {
                cleanFile = cleanFile.substr(slashInd+1);
            }

            std::array<Features,19*19> feats;
            std::array<Features,19*19> outFeats;
            extractFeatures(b,feats);

            constexpr decltype(feats[0].pack()) pack_obj = {};
            constexpr size_t bytesPer = pack_obj.size();
            std::array<uint8_t,bytesPer*19*19> packed;

            for(size_t sym = 0; sym<8; ++sym) {
                dihedralTranspose(feats,outFeats,sym);
                auto it = packed.begin();
                for(auto f: outFeats) {
                    assert(f.turnsSince <= 8);
                    assert(f.liberties <= 8);
                    assert(f.captureSize <= 8);
                    assert(f.selfAtariSize <= 8);
                    assert(f.libertiesAfter <= 8);

                    auto pack = f.pack();
                    assert(pack.size() == bytesPer);
                    it = std::copy(pack.begin(),pack.end(),it);
                }

                std::cout << "Writing move " << moveNum << " for "
                          << filename << std::endl;
                std::stringstream ss;
                ss << "feats-" << cleanFile << "-" << std::setfill('0')
                   << std::setw(3) << moveNum << "-" << sym << ".txt";
                auto s = ss.str();
                std::cout << "Output filename: " << s << std::endl;

                std::ofstream os(s.c_str(),std::ios::ate);

                bn::encode_b64(packed.begin(),packed.end(),
                               std::ostream_iterator<char>(os));

                it = packed.begin();
                for(auto& f: outFeats) {
                    std::array<uint8_t,2> pack;
                    std::copy(it,it+2,pack.begin());
                    it += 2;
                    Features written(pack);
                    if(f != written) {
                        std::cout << "{ ";
                        std::cout << (f.color == EMPTY ? '_' :
                                      f.color == MINE  ? 'M' : 'E');
                        std::cout << " " << f.turnsSince << " "
                                  << f.liberties << " "
                                  << f.captureSize << " "
                                  << f.selfAtariSize << " "
                                  << f.libertiesAfter << " "
                                  << (f.sensible ? 'S' : 's')
                                  << " }\n";
                        std::cout << "{ ";
                        std::cout << (written.color == EMPTY ? '_' :
                                      written.color == MINE  ? 'M' : 'E');
                        std::cout << " " << written.turnsSince << " "
                                  << written.liberties << " "
                                  << written.captureSize << " "
                                  << written.selfAtariSize << " "
                                  << written.libertiesAfter << " "
                                  << (written.sensible ? 'S' : 's')
                                  << " }\n";
                        assert(false);
                    }
                }

                auto p = dihedral(moveMade,sym);
                std::stringstream ss2;
                ss2 << "<" << p.first << "," << p.second << ">";
                auto parsed = readCoord(makeStream(ss2.str()));
                if(parsed != p) {
                    std::cerr << "Parse failed: " << p.first << ","
                              << p.second << " -> " << parsed.first << ","
                              << parsed.second << std::endl;
                    assert(false);
                }

                os << "\n\n<" << p.first << "," << p.second << ">\n";

                if(verbose) {
                    std::string metaname = "meta-" + s;
                    std::ofstream os2(metaname,std::ios::ate);
                    os2 << "Symmetry " << sym << ":\n";
                    os2 << "Board:\n" << b << "\n\n";
                    os2 << "Move made: " << moveMade.first << " "
                        << moveMade.second << std::endl;
                    os2 << "After symmetry: " << p.first << " "
                        << p.second << std::endl;
                    os2 << "\nColors:\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << (f.color == EMPTY ? '_' :
                                    f.color == MINE  ? 'o' :
                                  /*f.color == ENEMY*/ '*');
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nTurnsSince:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << f.turnsSince;
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nLiberties:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << f.liberties;
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nCaptureSize:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << f.captureSize;
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nSelfAtariSize:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << f.selfAtariSize;
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nLibertiesAfter:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << f.libertiesAfter;
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    os2 << "\n\nSensible:\n";

                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }
                    for(size_t y = 0; y < 19; ++y) {
                        os2 << "\n#";
                        for(size_t x = 0; x < 19; ++x) {
                            auto f = outFeats[IX({x,y})];
                            os2 << (f.sensible ? 'o' : '!');
                        }
                        os2 << "#";
                    }
                    os2 << "\n";
                    for(size_t i = 0; i < 21; ++i) {
                        os2 << '#';
                    }

                    tiny_cnn::vec_t planes;
                    convertToTCNNInput(outFeats,planes);

                    os2 << "\n";
                    for(size_t i=0; i < Features::NUM_CHANNELS;++i) {
                        os2 << "\nPlane " << i << ":\n";
                        for(size_t i = 0; i < 21; ++i) {
                            os2 << '#';
                        }
                        for(size_t y = 0; y < 19; ++y) {
                            os2 << "\n#";
                            for(size_t x = 0; x < 19; ++x) {
                                bool on = (planes[i*outFeats.size() +
                                                  IX({x,y})] >= 0.5);
                                os2 << (on ? '*' : '.');
                            }
                            os2 << '#';
                        }
                        os2 << '\n';
                        for(size_t i = 0; i < 21; ++i) {
                            os2 << '#';
                        }
                        os2 << '\n';
                    }
                }
            }
        };

        struct Cleanup {
            std::string name;
            Cleanup(std::string name_) : name(name_) {}

            ~Cleanup() {
                std::cerr << "DONE '''" << name << "'''\n";
                std::cerr.flush();
            }
        } cleaner_(filename);

        if(filename == "--") {
            gameServer(makeStream(std::cin),std::cout);
            return 0;

        } else if(filename == "--parsetest") {
            parse = true;
            if(argc < 2) {
                std::cout << "no file given\n";
                return -1;
            }
            filename = argv[2];

        } else if(filename == "--randombot") {
            RandomBot b;
            b.run();
            return 0;

        } else if(filename == "--onemovebot") {
            OneMoveBot b;
            b.run();
            return 0;

        } else if(filename == "--montecarlo") {
            int numThreads = 1;
            if(argc > 2) {
                try {
                    numThreads = std::stoi(argv[2]);
                } catch(std::exception e) {}
            }

            std::vector<int> polIds;
            std::vector<ptr<Policy>> pols;
            bool afs = false;
            if(argc > 3) {
                afs = (argv[3] == std::string("afs"));
                int i = afs ? 3 : 4;
                std::string path = afs ?
                    "~/private/omegago/sgf-parse/build/sgfparse" :
                    argv[0];
                for(; i < argc; ++i) {
                    try {
                        size_t whichOne = std::stoi(argv[i]);
                        pols.emplace_back(new Policy(path,whichOne));
                        polIds.push_back(whichOne);
                    } catch(std::exception e) {}
                }
            }
            std::cerr << pols.size() << " policies: [ ";
            for(auto id: polIds) {
                std::cerr << id << " ";
            }
            std::cerr << "]" << std::endl;

            MonteCarloBot b(numThreads,std::move(pols));
            b.run();
            return 0;

        } else if(filename == "--nnafs") {
            int whichOne = 1;
            if(argc > 2) {
                try {
                    whichOne = std::stoi(argv[2]);
                } catch(std::exception e) {}
            }
            NeuralNetBot b("~/private/omegago/sgf-parse/build/sgfparse",whichOne);
            b.run();
            return 0;

        } else if(filename == "--nn") {
            int whichOne = 1;
            if(argc > 2) {
                try {
                    whichOne = std::stoi(argv[2]);
                } catch(std::exception e) {}
            }
            NeuralNetBot b(argv[0],whichOne);
            b.run();
            return 0;

        } else if(filename == "--features") {
            printFeatures = true;
            assert(argc > 2);
            filename = argv[2];
            if(filename == "--print") {
                verbose = true;
                assert(argc > 3);
                filename = argv[3];
            }

        } else if(filename == "--shownet") {
            assert(argc > 3);
            return evalPolicyNet(argv[2],argc-3,argv+3);

        } else if(filename == "--train" || filename == "--testnet") {
            assert(argc > 3);
            return trainPolicyNet((filename == "--testnet"),argv[2],
                                  argc-3,argv+3);
        }

        std::cout << "File: " << filename << std::endl;

        std::ifstream is(filename.c_str());

        auto sgf = parseSgfFile(makeStream(is));
        if(!sgf) {
            std::cout << "Failed!\n";
        } else if(printFeatures) {
            size_t move = 0;
            Board b;
            Board newB;
            auto& game = *sgf->games.at(0);
            for(const auto& n: game.nodes) {
                if(newB.playSgfMove(n) && !newB.lastMove.pass) {
                    writeFeatures(move++,b,newB.lastMove.pt);
                }
                b = newB;
            }
        } else {
            std::cout << *sgf << std::endl;
            std::cout << "\n\n";

            Board b;
            std::cout << b << "\n\n\n";
            auto& game = *sgf->games.at(0);
            for(const auto& n: game.nodes) {
                if(parse) {
                    std::stringstream ss;
                    ss << b;
                    auto str = ss.str();
                    ss.str(str);
                    auto b2 = readBoard(makeStream(ss));
                    assert(b2);
                    /* assert(*b2 == b); */
                    ss.str("");
                    ss.seekp(0);
                    ss.seekg(0);
                    ss << *b2;
                    auto str2 = ss.str();
                    assert(ss.str() == str);
                }
                if(b.playSgfMove(n)) {
                    std::cout << b;
                    bool first = true;
                    Board::Cell color = b.blackToPlay ? Board::Black
                                                      : Board::White;
                    for(size_t y = 0; y < 19; ++y) {
                        for(size_t x = 0; x < 19; ++x) {
                            if(b.isLegal(color,x,y) &&
                               !b.isSensible(color,x,y)) {
                                std::cout << (first ? '\n' : ';')
                                          << "(" << x << "," << y
                                          << ")";
                                first = false;
                            }
                        }
                    }
                } else {
                    std::cout << "MOVE NOT PLAYED";
                }
                std::cout << "\n\n\n";
            }
        }
    }
    return 0;
}

