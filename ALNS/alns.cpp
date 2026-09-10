#include "alns.h"

ALNS::ALNS(const Instance& _inst, const Solution& _initial_sol) 
    : inst(_inst), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS::initOps() {
    destroy_ops.push_back(randomRemoval);
    destroy_ops.push_back(routeRemoval);
    destroy_ops.push_back([](Solution& sol, int q) {
        worstRemoval(sol, q);
    });
    destroy_ops.push_back([](Solution& sol, int q) {
        shawRemoval(sol, q);
    });

    repair_ops.push_back(greedyInsertion);
    repair_ops.push_back(regret2Insertion);
    repair_ops.push_back(regret3Insertion);
    repair_ops.push_back([](Solution& sol) {
        pGreedyInsertion(sol);
    });

    int num_destroy = destroy_ops.size();
    destroy_weights.assign(num_destroy, 1.0);

    int num_repair = repair_ops.size();
    repair_weights.assign(num_repair, 1.0);
}

int ALNS::selectDestroyOp() {
    std::discrete_distribution<int> distr(destroy_weights.begin(), destroy_weights.end());
    int selected_idx = distr(rng);
    return selected_idx;
}

int ALNS::selectRepairOp() {
    std::discrete_distribution<int> distr(repair_weights.begin(), repair_weights.end());
    int selected_idx = distr(rng);
    return selected_idx;
}

bool ALNS::accept(double cand_cost, double curr_cost, double current_temp) {
    double delta = cand_cost - curr_cost;
    if (delta <= 0) return true;
    
    double prob  = std::exp(-delta / current_temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    double random_val = distr(rng);

    if (random_val < prob) return true;

    return false;
}

void ALNS::updateWeights(int used_destroy_idx, int used_repair_idx, double score) {
    destroy_weights[used_destroy_idx] = (decay * destroy_weights[used_destroy_idx]) + ((1.0 - decay) * score);
    if (destroy_weights[used_destroy_idx] < 0.01)
        destroy_weights[used_destroy_idx] = 0.01;

    repair_weights[used_repair_idx] = (decay * repair_weights[used_repair_idx]) + ((1.0 - decay) * score);
    if (repair_weights[used_repair_idx] < 0.01)
        repair_weights[used_repair_idx] = 0.01;
}

Solution ALNS::solve(int max_iters, bool save_history) {
    double initial_d = current_sol.total_distance;
    start_temp = -(0.10 * initial_d) / std::log(0.5);
    double T = start_temp;
    int n_customers = inst.clients.size() - 1;
    if (save_history) {
        history.reserve(max_iters);
    }

    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    int iters_without_improvement = 0;

    for (int iter = 0; iter < max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng);
        int d_idx = selectDestroyOp();
        int r_idx = selectRepairOp();

        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        double score = w4;
        double cand_cost = cost(candidate);
        double curr_cost = cost(current_sol);
        double best_cost = cost(best_sol);

        if (cand_cost < best_cost) {
            best_sol = candidate;
            current_sol = candidate;
            score = w1;
            iters_without_improvement = 0;
        }
        else if (cand_cost == best_cost) {
            current_sol = candidate;
            score = w2;
        }
        else if (cand_cost < curr_cost) {
            current_sol = candidate;
            score = w2;
            iters_without_improvement = 0;
        }
        else if (accept(cand_cost, curr_cost, T)) {
            current_sol = candidate;
            score = w3;
        }
        else {
            iters_without_improvement++;
        }

        updateWeights(d_idx, r_idx, score);

        if (save_history) {
            IterationData data;
            data.iter = iter;
            data.best_vehicles = best_sol.used_vehicles;
            data.best_distance = best_sol.total_distance;
            data.curr_vehicles = current_sol.used_vehicles;
            data.curr_distance = current_sol.total_distance;
            data.d_idx = d_idx;
            data.r_idx = r_idx;
            data.score = score;
            data.temp = T;
            data.d_weights = destroy_weights;
            data.r_weights = repair_weights;

            history.emplace_back(data);
        }

        T = T * cooling_rate;
    }

    return best_sol;
}

void ALNS::exportMetrics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al abrir archivo para metricas: " << filename << std::endl;
        return;
    }

    file << "iter,best_veh,best_dist,curr_veh,curr_dist,d_op,r_op,score,temp";

    for (size_t i = 0; i < destroy_ops.size(); ++i) file << ",d_weight_" << i;
    for (size_t i = 0; i < repair_ops.size(); ++i) file << ",r_weight_" << i;
    file << "\n";

    for (const auto& data : history) {
        file << data.iter << ","
             << data.best_vehicles << ","
             << data.best_distance << ","
             << data.curr_vehicles << ","
             << data.curr_distance << ","
             << data.d_idx << ","
             << data.r_idx << ","
             << data.score << ","
             << data.temp;

        for (double w : data.d_weights) file << "," << w;
        for (double w : data.r_weights) file << "," << w;
        file << "\n";
    }

    file.close();
    std::cout << "-> Metricas exportadas a " << filename << "\n";
}
