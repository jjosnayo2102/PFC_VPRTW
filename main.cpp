#include <iostream>
#include <chrono>
#include <random>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "utils.h"

std::mt19937 rng;

int test_benchmark() {
    try {
        std::cout << "==========================================\n";
        std::cout << "    ALNS - VEHICLE ROUTING PROBLEM (VRPTW)\n";
        std::cout << "==========================================\n";

        std::string instance_file = "..\\solomon-100\\RC2\\rc201.txt";
        std::cout << "[1] Cargando instancia: " << instance_file << "...\n";
        Instance inst(instance_file);
        std::cout << "    -> Nodos cargados: " << inst.clients.size() << "\n";

        std::cout << "[2] Generando solucion inicial...\n";
        Solution initial_sol(inst);
        std::cout << initial_sol;

        int max_iterations = 25000;

        auto start_time = std::chrono::high_resolution_clock::now();
    
        // Elige uno
        Solution best_solution = solve_with_classic(inst, initial_sol, max_iterations, "..\\Results\\alns_metrics.csv");
        // Solution best_solution = solve_with_qlearning(inst, initial_sol, max_iterations, "..\\Results\\alns_qlearning_metrics.csv");

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

int minitest(const std::string& instance_file) {
    try {
        std::cout << "==========================================\n";
        std::cout << "    ALNS - VEHICLE ROUTING PROBLEM (VRPTW)\n";
        std::cout << "==========================================\n";

        std::cout << "[1] Cargando instancia: " << instance_file << "...\n";
        Instance inst(instance_file);
        std::cout << "    -> Nodos cargados: " << inst.clients.size() << "\n";

        std::cout << "[2] Generando solucion inicial...\n";
        Solution initial_sol(inst);
        std::cout << initial_sol;

        int max_iterations = 25000;

        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Elige uno
        // Solution best_solution = solve_with_classic(inst, initial_sol, max_iterations, "");
        Solution best_solution = solve_with_qlearning(inst, initial_sol, max_iterations, "");

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;

        std::cout << "\n==========================================\n";
        std::cout << "             BUSQUEDA TERMINADA\n";
        std::cout << "==========================================\n";
        std::cout << best_solution;
        std::cout << "------------------------------------------\n";
        std::cout << "Tiempo de CPU: " << diff.count() << " segundos\n";

        // GENERANDO MEJOR SOLUCION EXACTA
        std::cout << "\n======MEJOR=SOLUCION=DE=LA=INSTANCIA======\n";

        Solution empty_sol(inst);
        empty_sol.routes.clear();

        std::vector<bool> unassigned(N_NODES, true);
        unassigned[0] = false;
        int unassigned_count = N_NODES - 1;

        double best_cost = std::numeric_limits<double>::max();
        Solution best_sol(inst);

        std::cout << "Iniciando Fuerza Bruta (Branch and Bound)...\n";
        solveExact(empty_sol, unassigned, unassigned_count, best_cost, best_sol);

        std::cout << "\n=== OPTIMO GLOBAL ENCONTRADO ===\n";
        std::cout << best_sol;

    } catch (const std::exception& e) {
        std::cerr << "ERROR FATAL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int main(int, char**) {
    test_benchmark();
    return 0;
}