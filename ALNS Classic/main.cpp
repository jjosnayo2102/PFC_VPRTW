#include <iostream>
#include <chrono>
#include <random>
#include "alns.h"

std::mt19937 rng;

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
        std::cout << "[3] Iniciando ALNS por " << max_iterations << " iteraciones...\n";

        ALNS solver(inst, initial_sol);

        auto start_time = std::chrono::high_resolution_clock::now();
        
        Solution best_solution = solver.solve(max_iterations);
        solver.exportMetrics("..\\alns_metrics.csv");

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;

        std::cout << "\n==========================================\n";
        std::cout << "             BÚSQUEDA TERMINADA\n";
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