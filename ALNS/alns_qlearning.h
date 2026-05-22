#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random> 
#include "../Operators/operators.h"

struct IterationDataQL {
    int iter;
    int best_vehicles;
    double best_distance;
    int curr_vehicles;
    double curr_distance;
    int d_idx;
    int r_idx;
    double score;
    double reward;
    double temp;
    std::vector<double> d_weights;
    std::vector<double> r_weights;
};

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters);
        void exportMetrics(const std::string& filename);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;
        std::vector<IterationDataQL> history;
        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        double start_temp = 100.0;
        double cooling_rate = 0.9995;
        double w1 = 33.0, w2 = 13.0, w3 = 9.0, w4 = 0.0;
        double alpha = 0.1; 
        double gamma = 0.8; 
        int num_states = 3;
        std::vector<std::vector<double>> Q_destroy;
        std::vector<std::vector<double>> Q_repair;

        void initOps();
        bool accept(const Solution& candidate, double current_temp);

        std::vector<double> getSoftmaxProbabilities(const std::vector<double>& q_values, double tau);
        int selectOp(const std::vector<double>& probs);
};

#endif