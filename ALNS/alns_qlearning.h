#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random> // Asegúrate de incluir esto si usas generadores aleatorios como std::mt19937 en tu .cpp
#include "../Operators/operators.h"

struct IterationDataQL {
    int iter;
    int best_vehicles;
    double best_distance;
    int curr_vehicles;
    double curr_distance;
    int d_idx;
    int r_idx;
    double score;  // <-- NUEVO: Guardamos el score del Simulated Annealing
    double reward;
    double temp;
    std::vector<double> d_weights; // Ahora guarda Probabilidades Softmax para destrucción
    std::vector<double> r_weights; // Ahora guarda Probabilidades Softmax para reparación
};

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        // Misma firma que tu ALNS original
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters);
        void exportMetrics(const std::string& filename);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;

        std::vector<IterationDataQL> history;

        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        // --- Hiperparámetros ALNS / SA ---
        double start_temp = 100.0;
        double cooling_rate = 0.9995;
        double w1 = 33.0, w2 = 13.0, w3 = 9.0, w4 = 0.0;

        // --- Hiperparámetros Q-Learning ---
        double alpha = 0.1;   // Tasa de aprendizaje
        double gamma = 0.8;   // Factor de descuento (importancia del futuro)
        
        // --- Nueva definición de Entorno ---
        int num_states = 3;   // Estados por estancamiento: 0 (Mejorando), 1 (Estancado), 2 (Atrapado)
        
        // Tablas Q desacopladas para reducir explosión combinatoria
        std::vector<std::vector<double>> Q_destroy;
        std::vector<std::vector<double>> Q_repair;

        void initOps();
        bool accept(const Solution& candidate, double current_temp);

        // --- Nuevas funciones auxiliares para política Softmax ---
        std::vector<double> getSoftmaxProbabilities(const std::vector<double>& q_values, double tau);
        int selectOp(const std::vector<double>& probs);
};

#endif