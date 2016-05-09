#ifndef MONTECARLOTREE_HPP
#define MONTECARLOTREE_HPP

#include <vector>
#include <atomic>
#include <mutex>
#include <random>
#include "Board.hpp"
#include "LockedAtomicVector.hpp"

struct MonteCarloNode;

struct MonteCarloNode {
    static constexpr int virtLoss = 3;
    static constexpr int visitThresh = 10;
    static constexpr size_t maxNodeCount = 8000;
    static std::atomic<size_t> allocedNodes;
    const Board board;
    const std::vector<Board::Coord> moves;
    const bool isBlack;
    const size_t orientation;
    std::atomic<int> searches;
    std::atomic<int> totalVisits;
    std::atomic<bool> sentForPolicy;
    locked_atomic_vector<double> probabilities;
    locked_atomic_vector<int> outcomes;
    locked_atomic_vector<double> values;
    locked_atomic_vector<int> visits;
    locked_atomic_vector<MonteCarloNode*> children;
    std::atomic<MonteCarloNode*> pass;

    explicit MonteCarloNode(const Board& b,int sym);

    ~MonteCarloNode();

    void newOutcome(size_t move,int val);
    void updateValue(size_t move);
    void runSimulation(std::mt19937& rng);
    Board::Move mostVisits(std::mt19937& rng) const;
    Board::Move bestOutcome(std::mt19937& rng) const;
};

#endif

