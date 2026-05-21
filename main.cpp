#include <iostream>
#include <chrono>
#include <random>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "utils.h"

std::mt19937 rng;

int main(int, char**) {
    try {
        std::cout << "==========================================\n";
        std::cout << "    ALNS - VEHICLE ROUTING PROBLEM (VRPTW)\n";
        std::cout << "==========================================\n";

        std::string instance_file = "..\\solomon-100\\c2\\c201.txt";
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