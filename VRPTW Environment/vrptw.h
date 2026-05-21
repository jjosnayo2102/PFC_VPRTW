#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <algorithm>

extern std::mt19937 rng;

const int N_NODES = 101; // 100 clientes + deposito (0)
const int N_VEHICLES = 25;

struct Client {
    int id;
    double x;
    double y;
    double demand;
    double ready_time;
    double due_date;
    double service_time;

    Client(int _id, double _x, double _y, double d, double a, double b, double s): 
        id(_id), x(_x), y(_y), demand(d), ready_time(a), due_date(b), service_time(s)  {};
};

class Instance;

struct Route {
    std::vector<int> path;
    double load = 0.0;
    double distance = 0.0;

    // Para evaluaciones O(1)
    std::vector<double> arrival_times;
    std::vector<double> wait_times;
    std::vector<double> time_slacks;

    Route () {
        path.push_back(0);
        path.push_back(0);

        arrival_times.resize(2, 0.0);
        wait_times.resize(2, 0.0);
        time_slacks.resize(2, 0.0);
    }

    void recalculate(const Instance& inst);
};

class Instance {
    public:
        int capacity;
        std::vector<Client> clients;
        double dist_mat[N_NODES][N_NODES];
        bool is_reachable[N_NODES][N_NODES];

        Instance(const std::string& path) {
            clients.reserve(N_NODES);

            loadSolomon(path);
            precomputeDistances();
            precomputeFeasibility();
        }
    
    private:
        void loadSolomon(const std::string& path);
        void precomputeDistances();
        void precomputeFeasibility();
};

class Solution {
    public:
        const Instance& inst;
        std::vector<Route> routes;
        std::vector<int> unassigned;

        // Funcion objetivo f_1 y f_2
        int used_vehicles = 0;
        double total_distance = 0.0;

        Solution(const Instance& _inst) : inst(_inst) {
            generateInitialSolution();
        }

        friend std::ostream& operator<<(std::ostream& os, const Solution& s);
        bool operator<(const Solution& other) const;
        Solution& operator=(const Solution& other);

        void updateMetrics();

    private:
        void generateInitialSolution();
};

double cost(const Solution& sol);