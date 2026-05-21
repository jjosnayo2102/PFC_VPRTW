#include "vrptw.h"

void Route::recalculate(const Instance& inst) {
    load = 0.0;
    distance = 0.0;
    int L = path.size();

    arrival_times.resize(L, 0.0);
    wait_times.resize(L, 0.0);
    time_slacks.resize(L, 0.0);

    arrival_times[0] = 0.0;
    wait_times[0] = 0.0;
    double current_time = 0.0;
    
    for (size_t i = 0; i < L - 1; ++i) {
        int curr = path[i];
        int next = path[i+1];

        distance += inst.dist_mat[curr][next];
        load += inst.clients[curr].demand;

        double start_service = std::max(current_time, inst.clients[curr].ready_time);
        double arrival_at_next= start_service + inst.clients[curr].service_time + inst.dist_mat[curr][next];

        arrival_times[i + 1] = arrival_at_next;
        wait_times[i + 1] = std::max(0.0, inst.clients[next].ready_time - arrival_at_next);
        current_time = arrival_at_next;
    }

    load -= inst.clients[0].demand;

    int last = path[L - 1];
    double start_service_last = std::max(arrival_times[L - 1], inst.clients[last].ready_time);
    time_slacks[L - 1] = inst.clients[last].due_date - start_service_last;

    for (int i = L - 2; i >= 0; --i) {
        int node = path[i];
        double start_service = std::max(arrival_times[i], inst.clients[node].ready_time);
        double local_slack = inst.clients[node].due_date - start_service;

        time_slacks[i] = std::min(local_slack, time_slacks[i+1] + wait_times[i+1]);
    }
}   

void Instance::loadSolomon(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open())
        throw std::runtime_error("No se puede abrir " + path);

    std::string dummy;
    while (file >> dummy && dummy != "CAPACITY") {}

    int num_vehicles;
    file >> num_vehicles >> capacity;

    while (file >> dummy && dummy != "SERVICE") {}
    file >> dummy;

    for (int i = 0; i < N_NODES; ++i) {
        int id;
        double x, y, demand, ready_time, due_date, service_time;

        file >> id >> x >> y >> demand >> ready_time >> due_date >> service_time;
        clients.emplace_back(id, x, y, demand, ready_time, due_date, service_time);
    }

    file.close();
}

void Instance::precomputeDistances() {
    for (int i = 0; i < N_NODES; ++i) {
        for (int j = 0; j < N_NODES; ++j) {
            if (i == j)
                dist_mat[i][j] = 0.0;
            else {
                double dx = clients[i].x - clients[j].x;
                double dy = clients[i].y - clients[j].y;
                dist_mat[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }
    }
}

void Instance::precomputeFeasibility() {
    for (int i = 0; i < N_NODES; ++i) {
        for (int j = 0; j < N_NODES; ++j) {
            if (i == j) {
                is_reachable[i][j] = false;
                continue;
            }

            double earliest_arrival_at_j = clients[i].ready_time + clients[i].service_time + dist_mat[i][j];

            if (earliest_arrival_at_j > clients[j].due_date)
                is_reachable[i][j] = false;
            else
                is_reachable[i][j] = true;
        }
    }
}

std::ostream& operator<<(std::ostream& os, const Solution& s){
    os << "Vehiculos usados: " << s.used_vehicles << "  - Distancia total: " << s.total_distance << "\n";

    int r = 0;
    for (const Route& route : s.routes) {
        os << "Ruta [" << r << "]: ";
        for (const int& i : route.path) {
            if (i != 0)
                os << i << " ";
        }
        os << "\n";
        r++;
    }

    return os;
}

bool Solution::operator<(const Solution& other) const {
    if (this->used_vehicles < other.used_vehicles) return true;
    if (this->used_vehicles > other.used_vehicles) return false;

    return this->total_distance < other.total_distance;
}

Solution& Solution::operator=(const Solution& other) {
    if (this == &other) return *this;

    this->routes = other.routes;
    this->unassigned = other.unassigned;
    this->total_distance = other.total_distance;
    this->used_vehicles = other.used_vehicles;

    return *this;
}

void Solution::updateMetrics() {
    used_vehicles = 0;
    total_distance = 0.0;

    for (const Route& route: routes) {
        if (route.path.size() > 2) {
            used_vehicles++;
            total_distance += route.distance;
        }
    }
}

void Solution::generateInitialSolution() {
    std::vector<bool> visited(N_NODES, false);
    visited[0] = true;
    int unvisited = N_NODES - 1;

    while (unvisited > 0) {
        Route current_route;
        int current_node = 0;
        double current_time = 0.0;
        double current_load = 0.0;

        bool added_client = true;

        while (added_client) {
            added_client = false;
            int best_client = -1;
            double best_distance = std::numeric_limits<double>::max();

            for (int i = 1; i < N_NODES; ++i) {
                if (visited[i]) continue;
                if (!inst.is_reachable[current_node][i]) continue;
                if (current_load + inst.clients[i].demand > inst.capacity) continue;

                double arrival_time = current_time + inst.clients[current_node].service_time + inst.dist_mat[current_node][i];
                arrival_time = std::max(arrival_time, inst.clients[i].ready_time);

                if (arrival_time <= inst.clients[i].due_date) {
                    double return_time = arrival_time + inst.clients[i].service_time + inst.dist_mat[i][0];

                    if (return_time <= inst.clients[0].due_date) {
                        if (inst.dist_mat[current_node][i] < best_distance) {
                            best_distance = inst.dist_mat[current_node][i];
                            best_client = i;
                        }
                    }
                }
            }

            if (best_client != -1) {
                current_route.path.insert(current_route.path.end() - 1, best_client);
                visited[best_client] = true;
                unvisited--;
                
                current_load += inst.clients[best_client].demand;
                double arrival = current_time + inst.clients[current_node].service_time + inst.dist_mat[current_node][best_client];
                current_time = std::max(arrival, inst.clients[best_client].ready_time);
                
                current_route.distance += best_distance;
                current_route.load = current_load;
                
                current_node = best_client;
                added_client = true;
            }
            else {
                if (current_node == 0)
                    throw std::runtime_error("No hay clientes factibles desde el deposito.");
            }
        }
        
        current_route.distance += inst.dist_mat[current_node][0];
        routes.push_back(current_route);
    }

    updateMetrics();
}

double cost(const Solution& sol) {
    const double VEHICLE_COST = 10000.0;
    return (sol.used_vehicles * VEHICLE_COST) + sol.total_distance;
}