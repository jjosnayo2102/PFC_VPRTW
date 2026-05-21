#include "operators.h"

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