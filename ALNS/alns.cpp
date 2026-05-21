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

bool ALNS::accept(const Solution& candidate, double current_temp) {
    double current_cost = cost(current_sol);
    double candidate_cost = cost(candidate);

    // Mejor que la solucion actual?
    if (candidate_cost <= current_cost) return true;

    double delta = candidate_cost - current_cost;
    double prob  = std::exp(-delta / current_temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    double random_val = distr(rng);

    // Aceptada por el simulated annealing?
    if (random_val < prob) return true;

    return false; // Rechazada
}

void ALNS::updateWeights(int used_destroy_idx, int used_repair_idx, double score) {
    // Operadores destroy: \rho^- = \lambda * \rho^- + (1.0 - \lambda) * \psi
    destroy_weights[used_destroy_idx] = (decay * destroy_weights[used_destroy_idx]) + ((1.0 - decay) * score);
    if (destroy_weights[used_destroy_idx] < 0.01)
        destroy_weights[used_destroy_idx] = 0.01;

    // Operadores repair: \rho^+ = \lambda * \rho^+ + (1.0 - \lambda) * \psi
    repair_weights[used_repair_idx] = (decay * repair_weights[used_repair_idx]) + ((1.0 - decay) * score);
    if (repair_weights[used_repair_idx] < 0.01)
        repair_weights[used_repair_idx] = 0.01;
}

Solution ALNS::solve(int max_iters) {
    double T = start_temp;
    int n_customers = N_NODES - 1;
    history.reserve(max_iters);

    for (int iter = 0; iter < max_iters; ++iter) {
        Solution candidate = current_sol;

        // Grado de destruccion (cuantos clientes se busca eliminar)
        int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
        int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
        std::uniform_int_distribution<int> q_distr(q_min, q_max);
        int q = q_distr(rng);

        // Seleccion de operadores
        int d_idx = selectDestroyOp();
        int r_idx = selectRepairOp();

        // r(d(x))
        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        // Evaluacion y scores
        double score = w4;
        double cand_cost = cost(candidate);
        double curr_cost = cost(current_sol);
        double best_cost = cost(best_sol);

        if (cand_cost < best_cost) {
            // Nuevo mejor global
            best_sol = candidate;
            current_sol = candidate;
            score = w1;
        }
        else if (cand_cost < curr_cost) {
            // Nuevo mejor actual
            current_sol = candidate;
            score = w2;
        }
        else if (accept(candidate, T)) {
            // Solucion aceptada 
            current_sol = candidate;
            score = w3;
        }
        else {
            // Mala solucion
        }

        // Actualizacion de pesos
        updateWeights(d_idx, r_idx, score);

        // Guardando metricas de la iteracion actual
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

        // Actualizacion de temperatura
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