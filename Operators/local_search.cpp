#include "operators.h"

// Verifica factibilidad de ventanas de tiempo y capacidad de una ruta
bool isRouteFeasible(const Route& route, const Instance& inst) {
    if (route.path.size() <= 2) return true;

    double current_time = 0.0;
    double current_load = 0.0;

    for (size_t i = 0; i < route.path.size() - 1; ++i) {
        int curr = route.path[i];
        int next = route.path[i + 1];

        // Capacidad (acumulamos demanda del siguiente nodo si es cliente)
        if (next != 0) {
            current_load += inst.clients[next].demand;
            if (current_load > inst.capacity) return false;
        }

        // Ventana de tiempo
        double arrival_time = current_time + inst.clients[curr].service_time
                            + inst.dist_mat[curr][next];
        if (arrival_time > inst.clients[next].due_date + 1e-6) return false;

        current_time = std::max(arrival_time, inst.clients[next].ready_time);
    }
    return true;
}

// 2-opt intra-ruta: invierte segmentos dentro de cada ruta para reducir distancia
bool intraRoute2Opt(Solution& sol) {
    bool any_improved = false;

    for (Route& route : sol.routes) {
        if (route.path.size() <= 4) continue;  // Minimo 0-a-b-0

        bool route_improved = true;
        while (route_improved) {
            route_improved = false;
            for (size_t i = 1; i < route.path.size() - 2 && !route_improved; ++i) {
                for (size_t j = i + 1; j < route.path.size() - 1; ++j) {
                    int prev_i = route.path[i - 1];
                    int node_i = route.path[i];
                    int node_j = route.path[j];
                    int next_j = route.path[j + 1];

                    double old_cost = sol.inst.dist_mat[prev_i][node_i]
                                    + sol.inst.dist_mat[node_j][next_j];
                    double new_cost = sol.inst.dist_mat[prev_i][node_j]
                                    + sol.inst.dist_mat[node_i][next_j];

                    if (new_cost < old_cost - 1e-6) {
                        // Intentar inversion
                        std::reverse(route.path.begin() + i,
                                     route.path.begin() + j + 1);
                        route.recalculate(sol.inst);

                        if (isRouteFeasible(route, sol.inst)) {
                            route_improved = true;
                            any_improved = true;
                        } else {
                            // Revertir
                            std::reverse(route.path.begin() + i,
                                         route.path.begin() + j + 1);
                            route.recalculate(sol.inst);
                        }
                    }
                }
            }
        }
    }

    if (any_improved) sol.updateMetrics();
    return any_improved;
}

// Intenta eliminar la ruta mas pequena redistribuyendo sus clientes
// Prueba multiples estrategias de reparacion
bool tryRouteElimination(Solution& sol) {
    // Encontrar la ruta activa mas pequena
    int min_sz = static_cast<int>(sol.inst.clients.size());
    int min_r = -1;
    for (size_t i = 0; i < sol.routes.size(); ++i) {
        int sz = static_cast<int>(sol.routes[i].path.size()) - 2;
        if (sz > 0 && sz < min_sz) { min_sz = sz; min_r = i; }
    }
    if (min_r == -1 || min_sz > 15) return false;

    Solution backup = sol;

    // Mover clientes de la ruta objetivo a unassigned
    for (size_t i = 1; i < sol.routes[min_r].path.size() - 1; ++i)
        sol.unassigned.push_back(sol.routes[min_r].path[i]);

    // Eliminar la ruta
    sol.routes.erase(sol.routes.begin() + min_r);
    sol.updateMetrics();

    // Intentar con greedy
    {
        Solution attempt = sol;
        greedyInsertion(attempt);
        attempt.updateMetrics();
        if (attempt.unassigned.empty() && attempt.used_vehicles < backup.used_vehicles) {
            sol = attempt;
            return true;
        }
    }

    // Intentar con regret-2
    {
        Solution attempt = sol;
        regret2Insertion(attempt);
        attempt.updateMetrics();
        if (attempt.unassigned.empty() && attempt.used_vehicles < backup.used_vehicles) {
            sol = attempt;
            return true;
        }
    }

    // Intentar con regret-3
    {
        Solution attempt = sol;
        regret3Insertion(attempt);
        attempt.updateMetrics();
        if (attempt.unassigned.empty() && attempt.used_vehicles < backup.used_vehicles) {
            sol = attempt;
            return true;
        }
    }

    sol = backup;
    return false;
}
