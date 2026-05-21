#include "operators.h"

/// Quitamos clientes aleatorios de rutas
void randomRemoval(Solution& sol, int q){
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N_NODES - 1){
        std::vector<int> active_routes;
        for (int i = 0; i < sol.routes.size(); ++i) {
            if (sol.routes[i].path.size() > 2) {
                active_routes.push_back(i);
            }
        }

        if (active_routes.empty()) break;

        std::uniform_int_distribution<int> route_distr(0, active_routes.size() - 1);
        int r_idx = active_routes[route_distr(rng)];
        Route& route = sol.routes[r_idx];

        std::uniform_int_distribution<int> client_distr(1, route.path.size() - 2);
        int c_idx = client_distr(rng);
        int client_id = route.path[c_idx];
        
        sol.unassigned.push_back(client_id);
        route.path.erase(route.path.begin() + c_idx);
        route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}

// Eliminamos rutas aleatorias
void routeRemoval(Solution& sol, int q){
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N_NODES - 1) {
        std::vector<int> active_routes;
        for (int i = 0; i < sol.routes.size(); ++i)
            if (sol.routes[i].path.size() > 2)
                active_routes.push_back(i);
        
        if (active_routes.empty()) break;

        std::uniform_int_distribution<int> distr(0, active_routes.size() - 1);
        int chosen_idx = active_routes[distr(rng)];
        Route& route = sol.routes[chosen_idx];
        
        for (size_t i = 1; i < route.path.size() - 1; ++i) {
            sol.unassigned.push_back(route.path[i]);
            removed++;
        }

        route.path.clear();
        route.path.push_back(0);
        route.path.push_back(0);

        route.recalculate(sol.inst);
    }

    sol.updateMetrics();
}

// Eliminamos clientes mas ineficientes
void worstRemoval(Solution& sol, int q, double p){
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N_NODES - 1) {
        std::vector<RemovalCandidate> candidates;

        for (int r = 0; r < sol.routes.size(); ++r) {
            Route& route = sol.routes[r];
            if (route.path.size() <= 2) continue;

            for (int i = 1; i < route.path.size() - 1; ++i) {
                int prev = route.path[i-1];
                int curr = route.path[i];
                int next = route.path[i+1];

                double cost = sol.inst.dist_mat[prev][curr] + sol.inst.dist_mat[curr][next] - sol.inst.dist_mat[prev][next];
                candidates.push_back({r, i, cost});
            }
        }

        if (candidates.empty()) break;

        std::sort(candidates.begin(), candidates.end(),
            [](const RemovalCandidate& a, const RemovalCandidate& b) {
                return a.deviation_cost > b.deviation_cost;
            });
        
        std::uniform_real_distribution<double> distr(0.0, 1.0);
        double y = distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y,p) * candidates.size());
        if (chosen_idx >= candidates.size())
            chosen_idx = candidates.size() - 1;
        
        RemovalCandidate chosen = candidates[chosen_idx];
        Route& target_route = sol.routes[chosen.route_idx];

        sol.unassigned.push_back(target_route.path[chosen.node_idx]);
        target_route.path.erase(target_route.path.begin() + chosen.node_idx);
        target_route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}

// Eliminamos clientes mas parecidos
void shawRemoval(Solution& sol, int q, double p){
    const double w_dist = 9.0;
    const double w_time = 3.0;
    const double w_demand = 2.0;

    int removed = 0;
    std::vector<int> removed_clients;

    std::vector<int> active_routes;
    for (int i = 0; i < sol.routes.size(); ++i)
        if (sol.routes[i].path.size() > 2)
            active_routes.push_back(i);
    
    if (active_routes.empty()) return;

    std::uniform_int_distribution<int> r_distr(0, active_routes.size() - 1);
    int first_r_idx = active_routes[r_distr(rng)];
    Route& first_route = sol.routes[first_r_idx];

    std::uniform_int_distribution<int> c_distr(1, first_route.path.size() - 2);
    int first_c_idx = c_distr(rng);
    int first_client = first_route.path[first_c_idx];

    removed_clients.push_back(first_client);
    sol.unassigned.push_back(first_client);
    first_route.path.erase(first_route.path.begin() + first_c_idx);
    first_route.recalculate(sol.inst);
    removed++;

    while (removed < q && sol.unassigned.size() < N_NODES - 1) {
        std::uniform_int_distribution<int> base_distr(0, removed_clients.size() - 1);
        int base_client_id = removed_clients[base_distr(rng)];
        const Client& base_client = sol.inst.clients[base_client_id];

        std::vector<RelatednessCandidate> candidates;
        for (int r = 0; r < sol.routes.size(); ++r) {
            Route& route = sol.routes[r];
            if (route.path.size() <= 2) continue;

            for (int i = 1; i < route.path.size() - 1; ++i) {
                int target_id = route.path[i];
                const Client& target_client = sol.inst.clients[target_id];

                double r_score = 
                    (w_dist * sol.inst.dist_mat[base_client_id][target_id]) +
                    (w_time * std::abs(base_client.ready_time - target_client.ready_time)) +
                    (w_demand * std::abs(base_client.demand - target_client.demand));
                
                candidates.push_back({r, i, target_id, r_score});
            }
        }

        if (candidates.empty()) break;

        std::sort(candidates.begin(), candidates.end(),
            [](const RelatednessCandidate& a, const RelatednessCandidate& b) {
                return a.relatedness > b.relatedness;
            });
        
        std::uniform_real_distribution<double> y_distr(0.0, 1.0);
        double y = y_distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y, p) * candidates.size());
        if (chosen_idx >= candidates.size())
            chosen_idx = candidates.size() - 1;
        
        RelatednessCandidate chosen = candidates[chosen_idx];
        Route& target_route = sol.routes[chosen.route_idx];

        removed_clients.push_back(chosen.client_id);
        sol.unassigned.push_back(chosen.client_id);
        target_route.path.erase(target_route.path.begin() + chosen.node_idx);
        target_route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}

// Insertamos lo que minimiza el aumento inmediato de costo
void greedyInsertion(Solution& sol){
    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    int prev = route.path[i];
                    int next = route.path[i + 1];
                    if (!sol.inst.is_reachable[prev][client_id]) continue;
                    if (!sol.inst.is_reachable[client_id][next]) continue;

                    double arrival_u = route.arrival_times[i] +
                                       sol.inst.clients[prev].service_time +
                                       sol.inst.dist_mat[prev][client_id];
                    if (arrival_u > u_client.due_date) continue;

                    double start_u = std::max(arrival_u, u_client.ready_time);
                    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
                    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
                    if (delay > route.wait_times[i+1] + route.time_slacks[i+1]) continue;
                
                    double delta_cost = sol.inst.dist_mat[prev][client_id] +
                                        sol.inst.dist_mat[client_id][next] -
                                        sol.inst.dist_mat[prev][next];
                    if (delta_cost < best_cost) {
                        best_cost = delta_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];
            
            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned.erase(sol.unassigned.begin() + best_client_idx_in_unassigned);
        }
        else {
            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                int forced_client = sol.unassigned[0];
                sol.routes.back().path.insert(sol.routes.back().path.begin() + 1, forced_client);
                sol.routes.back().recalculate(sol.inst);
                sol.unassigned.erase(sol.unassigned.begin());
            } 
            else
                sol.routes.push_back(Route());
        }
    }

    sol.updateMetrics();
}   

// Greedy anticipando la segunda mejor posicion del cliente
void regret2Insertion(Solution& sol){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            double best_cost = std::numeric_limits<double>::max();
            double second_best_cost = std::numeric_limits<double>::max();
            int local_best_route = -1;
            int local_best_pos = -1;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    int prev = route.path[i];
                    int next = route.path[i+1];
                    if (!sol.inst.is_reachable[prev][client_id]) continue;
                    if (!sol.inst.is_reachable[client_id][next]) continue;

                    double arrival_u = route.arrival_times[i] + 
                                       sol.inst.clients[prev].service_time +
                                       sol.inst.dist_mat[prev][client_id];
                    if (arrival_u > u_client.due_date) continue;

                    double start_u = std::max(arrival_u, u_client.ready_time);
                    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
                    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
                    if (delay > route.wait_times[i + 1] + route.time_slacks[i + 1]) continue;

                    double delta_cost = sol.inst.dist_mat[prev][client_id] +
                                        sol.inst.dist_mat[client_id][next] -
                                        sol.inst.dist_mat[prev][next];
                    if (delta_cost < best_cost) {
                        second_best_cost = best_cost;
                        best_cost = delta_cost;
                        local_best_route = r;
                        local_best_pos = i+1;
                    }
                    else if (delta_cost < second_best_cost)
                        second_best_cost = delta_cost;
                }
            }

            if (best_cost != std::numeric_limits<double>::max()) {
                double regret = second_best_cost - best_cost;

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = local_best_route;
                    global_best_pos = local_best_pos;
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned.erase(sol.unassigned.begin() + best_client_idx_in_unassigned);
        }
        else {
            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                int forced_client = sol.unassigned[0];
                sol.routes.back().path.insert(sol.routes.back().path.begin() + 1, forced_client);
                sol.routes.back().recalculate(sol.inst);
                sol.unassigned.erase(sol.unassigned.begin());
            } 
            else
                sol.routes.push_back(Route());
        }
    }
    
    sol.updateMetrics();
}

// Regret-2 pero para las m=3 mejores rutas
void regret3Insertion(Solution& sol){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            std::vector<RouteInsertion> best_per_route;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                double best_cost_in_r = std::numeric_limits<double>::max();
                int best_pos_in_r = -1;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    int prev = route.path[i];
                    int next = route.path[i+1];
                    if (!sol.inst.is_reachable[prev][client_id]) continue;
                    if (!sol.inst.is_reachable[client_id][next]) continue;

                    double arrival_u = route.arrival_times[i] + 
                                       sol.inst.clients[prev].service_time +
                                       sol.inst.dist_mat[prev][client_id];
                    if (arrival_u > u_client.due_date) continue;

                    double start_u = std::max(arrival_u, u_client.ready_time);
                    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
                    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
                    if (delay > route.wait_times[i + 1] + route.time_slacks[i + 1]) continue;

                    double delta_cost = sol.inst.dist_mat[prev][client_id] +
                                        sol.inst.dist_mat[client_id][next] -
                                        sol.inst.dist_mat[prev][next];
                    if (delta_cost < best_cost_in_r) {
                        best_cost_in_r = delta_cost;
                        best_pos_in_r = i + 1;
                    }
                }

                if (best_cost_in_r != std::numeric_limits<double>::max())
                    best_per_route.push_back({static_cast<int>(r), best_pos_in_r, best_cost_in_r});
            }

            if (!best_per_route.empty()){
                std::sort(best_per_route.begin(), best_per_route.end(),
                    [](const RouteInsertion& a, const RouteInsertion& b) {
                        return a.cost < b.cost;
                    });

                double best_cost = best_per_route[0].cost;
                double regret = 0.0;

                int m = std::min(3, static_cast<int>(best_per_route.size()));
                for (int k = 1; k < m; ++k)
                    regret += (best_per_route[k].cost - best_cost);

                int missing_routes = 3 - m;
                regret += missing_routes * 10000.0;

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = best_per_route[0].route_idx;
                    global_best_pos = best_per_route[0].insert_pos; 
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned.erase(sol.unassigned.begin() + best_client_idx_in_unassigned);
        }
        else {
            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                int forced_client = sol.unassigned[0];
                sol.routes.back().path.insert(sol.routes.back().path.begin() + 1, forced_client);
                sol.routes.back().recalculate(sol.inst);
                sol.unassigned.erase(sol.unassigned.begin());
            } 
            else
                sol.routes.push_back(Route());
        }
    }
    
    sol.updateMetrics();
}

// Greedy con ruido
void pGreedyInsertion(Solution& sol, double eta){
    std::uniform_real_distribution<double> noise_distr(1.0 - eta, 1.0 + eta);

    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    int prev = route.path[i];
                    int next = route.path[i + 1];
                    if (!sol.inst.is_reachable[prev][client_id]) continue;
                    if (!sol.inst.is_reachable[client_id][next]) continue;

                    double arrival_u = route.arrival_times[i] +
                                       sol.inst.clients[prev].service_time +
                                       sol.inst.dist_mat[prev][client_id];
                    if (arrival_u > u_client.due_date) continue;

                    double start_u = std::max(arrival_u, u_client.ready_time);
                    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
                    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
                    if (delay > route.wait_times[i+1] + route.time_slacks[i+1]) continue;
                
                    double real_cost = sol.inst.dist_mat[prev][client_id] +
                                        sol.inst.dist_mat[client_id][next] -
                                        sol.inst.dist_mat[prev][next];
                    double perturbed_cost = real_cost * noise_distr(rng);
                    if (perturbed_cost < best_cost) {
                        best_cost = perturbed_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];

            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned.erase(sol.unassigned.begin() + best_client_idx_in_unassigned);
        }
        else {
            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                int forced_client = sol.unassigned[0];
                sol.routes.back().path.insert(sol.routes.back().path.begin() + 1, forced_client);
                sol.routes.back().recalculate(sol.inst);
                sol.unassigned.erase(sol.unassigned.begin());
            } 
            else
                sol.routes.push_back(Route());
        }
    }
    
    sol.updateMetrics();
}