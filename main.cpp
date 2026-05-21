#include <iostream>
#include <chrono>
#include <random>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"

std::mt19937 rng;

Solution solve_with_classic(const Instance& inst, const Solution& sol, int max_iters) {
    std::cout << "[3] Iniciando ALNS por " << max_iters << " iteraciones...\n";
    ALNS solver(inst, sol);
    Solution best_solution = solver.solve(max_iters);
    solver.exportMetrics("..\\Results\\alns_metrics.csv");
    return best_solution;
}

Solution solve_with_qlearning(const Instance& inst, const Solution& sol, int max_iters) {
    std::cout << "[3] Iniciando ALNS con Q-Learning por " << max_iters << " iteraciones...\n";
    ALNS_QLearning solver(inst, sol);
    Solution best_solution = solver.solve(max_iters);
    solver.exportMetrics("..\\Results\\alns_qlearning_metrics.csv");
    return best_solution;
}

int main(int, char**) {
    try {
        std::cout << "==========================================\n";
        std::cout << "    ALNS - VEHICLE ROUTING PROBLEM (VRPTW)\n";
        std::cout << "==========================================\n";

        std::string instance_file = "..\\solomon-100\\R1\\r101.txt";
        std::cout << "[1] Cargando instancia: " << instance_file << "...\n";
        Instance inst(instance_file);
        std::cout << "    -> Nodos cargados: " << inst.clients.size() << "\n";

        std::cout << "[2] Generando solucion inicial...\n";
        Solution initial_sol(inst);
        std::cout << initial_sol;

        int max_iterations = 25000;

        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Elige uno
        Solution best_solution = solve_with_classic(inst, initial_sol, max_iterations);
        //Solution best_solution = solve_with_qlearning(inst, initial_sol, max_iterations);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;

        std::cout << "\n==========================================\n";
        std::cout << "             BUSQUEDA TERMINADA\n";
        std::cout << "==========================================\n";
        std::cout << best_solution;
        std::cout << "------------------------------------------\n";
        std::cout << "Tiempo de CPU: " << diff.count() << " segundos\n";

    } catch (const std::exception& e) {
        std::cerr << "ERROR FATAL: " << e.what() << "\n";
        return 1;
    }

    return 0;
}