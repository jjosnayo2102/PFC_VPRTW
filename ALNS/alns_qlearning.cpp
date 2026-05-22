#include "alns_qlearning.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

ALNS_QLearning::ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol) 
    : inst(_inst), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS_QLearning::initOps() {
    destroy_ops.push_back(randomRemoval);
    destroy_ops.push_back(routeRemoval);
    destroy_ops.push_back([](Solution& sol, int q) { worstRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { shawRemoval(sol, q); });

    repair_ops.push_back(greedyInsertion);
    repair_ops.push_back(regret2Insertion);
    repair_ops.push_back(regret3Insertion);
    repair_ops.push_back([](Solution& sol) { pGreedyInsertion(sol); });

    // 0: Mejorando, 1: Estancado, 2: Atrapado
    num_states = 3; 
    Q_destroy.assign(num_states, std::vector<double>(destroy_ops.size(), 0.0));
    Q_repair.assign(num_states, std::vector<double>(repair_ops.size(), 0.0));
}

std::vector<double> ALNS_QLearning::getSoftmaxProbabilities(const std::vector<double>& q_values, double tau) {
    std::vector<double> probs(q_values.size());
    double max_q = *std::max_element(q_values.begin(), q_values.end());
    double sum = 0.0;
    
    for (size_t i = 0; i < q_values.size(); ++i) {
        probs[i] = std::exp((q_values[i] - max_q) / tau);
        sum += probs[i];
    }
    for (size_t i = 0; i < probs.size(); ++i) {
        probs[i] /= sum;
    }
    return probs;
}

int ALNS_QLearning::selectOp(const std::vector<double>& probs) {
    std::discrete_distribution<int> distr(probs.begin(), probs.end());
    return distr(rng);
}

bool ALNS_QLearning::accept(const Solution& candidate, double T) {
    double delta = cost(candidate) - cost(current_sol);
    if (delta <= 0) return true;
    double prob = std::exp(-delta / T);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    return distr(rng) < prob; 
}

Solution ALNS_QLearning::solve(int max_iters) {
    double T = start_temp;
    

    double tau = 5.0;  
    double tau_min = 0.1;  
    double tau_cooling = 0.995;

    int n_customers = inst.clients.size() - 1;
    history.reserve(max_iters);

    int current_state = 0; 
    int iters_without_improvement = 0;

    for (int iter = 0; iter < max_iters; ++iter) {
        Solution candidate = current_sol;
        
        int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
        int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
        std::uniform_int_distribution<int> q_distr(q_min, q_max);
        int q = q_distr(rng);
        
        std::vector<double> d_probs = getSoftmaxProbabilities(Q_destroy[current_state], tau);
        std::vector<double> r_probs = getSoftmaxProbabilities(Q_repair[current_state], tau);
        
        int d_idx = selectOp(d_probs);
        int r_idx = selectOp(r_probs);
        
        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);
        
        double reward = w4;
        bool global_improved = false;
        
        double cand_cost = cost(candidate);
        double curr_cost = cost(current_sol);
        double best_cost = cost(best_sol);

        if (cand_cost < best_cost) {
            best_sol = candidate;
            current_sol = candidate;
            reward = w1; 
            global_improved = true;
        }
        else if (cand_cost < curr_cost) {
            current_sol = candidate;
            reward = w2; 
        }
        else if (accept(candidate, T)) { 
            current_sol = candidate;
            reward = w3; 
        }

        if (global_improved) {
            iters_without_improvement = 0;
        } else {
            iters_without_improvement++;
        }

        int next_state = 0; // Mejorando
        if (iters_without_improvement > 150) {
            next_state = 2; // Atrapado
        } else if (iters_without_improvement > 50) {
            next_state = 1; // Estancado
        }

        double max_next_q_d = *std::max_element(Q_destroy[next_state].begin(), Q_destroy[next_state].end());
        Q_destroy[current_state][d_idx] += alpha * (reward + gamma * max_next_q_d - Q_destroy[current_state][d_idx]);

        double max_next_q_r = *std::max_element(Q_repair[next_state].begin(), Q_repair[next_state].end());
        Q_repair[current_state][r_idx] += alpha * (reward + gamma * max_next_q_r - Q_repair[current_state][r_idx]);

        current_state = next_state;
        
        IterationDataQL data;
        data.iter = iter;
        data.best_vehicles = best_sol.used_vehicles;
        data.best_distance = best_sol.total_distance;
        data.curr_vehicles = current_sol.used_vehicles;
        data.curr_distance = current_sol.total_distance;
        data.d_idx = d_idx;
        data.r_idx = r_idx;
        data.reward = reward; 
        data.temp = T;
        data.d_weights = d_probs; 
        data.r_weights = r_probs; 
        history.emplace_back(data);
        
        T = T * cooling_rate; // Cierra la aceptacion de peores rutas
        tau = std::max(tau_min, tau * tau_cooling); // Cierra la exploracion de operadores al azar
    }
    return best_sol;
}

void ALNS_QLearning::exportMetrics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al abrir archivo para metricas: " << filename << std::endl;
        return;
    }
    file << "iter,best_veh,best_dist,curr_veh,curr_dist,d_op,r_op,reward,temp";
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
             << data.reward << ","
             << data.temp;
        for (double w : data.d_weights) file << "," << w;
        for (double w : data.r_weights) file << "," << w;
        file << "\n";
    }
    file.close();
    std::cout << "-> Metricas exportadas a " << filename << "\n";
}