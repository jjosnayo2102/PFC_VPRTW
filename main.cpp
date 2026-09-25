#include <iostream>
#include <ctime>
#include <chrono>
#include <string>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "Utils/utils.h"

std::mt19937 rng;

#ifdef _WIN32
#include <windows.h>
static double get_cpu_time() {
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER k, u;
        k.LowPart = ftKernel.dwLowDateTime;     k.HighPart = ftKernel.dwHighDateTime;
        u.LowPart = ftUser.dwLowDateTime;       u.HighPart = ftUser.dwHighDateTime;
        return static_cast<double>(k.QuadPart + u.QuadPart) / 1e7;
    }
    return 0.0;
}
#else
#include <time.h>
static double get_cpu_time() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}
#endif

static unsigned parse_seed(const std::string& s, unsigned fallback) {
    try {
        return static_cast<unsigned>(std::stoul(s));
    } catch (...) {
        return fallback;
    }
}

int test_benchmark() {
    try {
        unsigned manual_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        rng.seed(manual_seed);

        std::cout << "==========================================\n";
        std::cout << "    ALNS - VEHICLE ROUTING PROBLEM (VRPTW)\n";
        std::cout << "==========================================\n";
        std::cout << "[INFO] Seed utilizada: " << manual_seed << "\n";

        std::string instance_file = "../solomon-100/rc1/rc107.txt";
        std::cout << "[1] Cargando instancia: " << instance_file << "...\n";
        Instance inst(instance_file);
        std::cout << "    -> Nodos cargados: " << inst.clients.size() << "\n";

        std::cout << "[2] Generando solucion inicial...\n";
        Solution initial_sol(inst);
        std::cout << initial_sol;

        int max_iterations = 25000;

        double start_cpu_time = get_cpu_time();
    
        // Elige uno
        Solution best_solution = solve_with_classic(inst, initial_sol, max_iterations);
        // Solution best_solution = solve_with_qlearning(inst, initial_sol, max_iters);

        double cpu_time_used = get_cpu_time() - start_cpu_time;

        std::cout << "\n==========================================\n";
        std::cout << "             BUSQUEDA TERMINADA\n";
        std::cout << "==========================================\n";
        std::cout << best_solution;
        std::cout << "------------------------------------------\n";
        std::cout << "Tiempo de CPU: " << cpu_time_used << " segundos\n";

    } catch (const std::exception& e) {
        std::cerr << "ERROR FATAL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "[MANUAL]\n";
        test_benchmark();
    }
    else if (argc >= 4) {
        try {
            std::string instance_file = argv[1];
            std::string algorithm = argv[2];        // "CLASSIC" / "QLEARNING"
            int max_iters = std::stoi(argv[3]);
            std::string run_id = (argc >= 5) ? argv[4] : "0";

            unsigned seed = (argc >= 6) ? parse_seed(argv[5], parse_seed(run_id, 1u))
                                        : parse_seed(run_id, 1u);
            rng.seed(seed);

            Instance inst(instance_file);
            Solution initial_sol(inst);

            size_t last_slash = instance_file.find_last_of("/\\");
            size_t last_dot = instance_file.find_last_of(".");
            std::string inst_name = instance_file.substr(last_slash + 1, last_dot - last_slash - 1);
            
            std::string metrics_file = "../Results/" + algorithm + "/metrics/" + algorithm + "_" + inst_name + "_metrics_run" + run_id + ".csv";
            std::string routes_file = "../Results/" + algorithm + "/routes/" + algorithm + "_" + inst_name + "_metrics_run" + run_id + ".csv";
            
            double start_cpu = get_cpu_time();

            // Declaramos la variable para almacenar la mejor solucion retornada
            Solution best_solution = initial_sol;

            if (algorithm == "CLASSIC") {
                best_solution = solve_with_classic(inst, initial_sol, max_iters, metrics_file, routes_file);
            }
            else if (algorithm == "QLEARNING") {
                best_solution = solve_with_qlearning(inst, initial_sol, max_iters, metrics_file, routes_file);
            }
            else {
                std::cerr << "Algoritmo desconocido: " << algorithm << "\n";
                return 1;
            }

            double cpu_time_used = get_cpu_time() - start_cpu;
            
            // Imprimir el resultado en el formato exacto que espera la expresion regular en Python
            std::cout << "[FINAL_RESULT] Veh: " << best_solution.used_vehicles 
                      << ", Dist: " << best_solution.total_distance << "\n";

            std::cout << "[INFO] Seed: " << seed << "\n";
            std::cout << "Tiempo de CPU real: " << cpu_time_used << " segundos\n";

        } catch (const std::exception& e) {
            std::cerr << "ERROR FATAL: " << e.what() << "\n";
            return 1;
        }
    }
    else {
        std::cerr << "Uso incorrecto. Argumentos esperados: <instancia> <CLASSIC|QLEARNING> <iteraciones> [run_id] [seed]\n";
        return 1;
    }

    return 0;
}