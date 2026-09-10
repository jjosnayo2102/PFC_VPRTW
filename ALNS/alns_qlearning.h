#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <deque>
#include "../Operators/operators.h"

// Resultado del criterio de aceptacion (acopla la recompensa a la salida real del ALNS)
enum class Outcome {
    REJECTED     = 0,   // el candidato se descarta
    ACCEPTED_SA  = 1,   // peor que la actual, pero aceptado por Simulated Annealing
    IMPROVED     = 2,   // mejora la solucion actual
    NEW_BEST     = 3    // mejora el mejor global
};

struct IterationDataQL {
    int iter;
    int best_vehicles;
    double best_distance;
    int curr_vehicles;
    double curr_distance;
    int d_idx;
    int r_idx;
    double reward;
    double temp;
    double epsilon;
    int outcome;
    double opportunity_cost;
};

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters, bool save_history = false);
        void exportMetrics(const std::string& filename);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;
        std::vector<IterationDataQL> history;

        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        // ---------------- Simulated Annealing ----------------
        double start_temp = 0.0;        // calibrada sobre la distancia inicial
        double cooling_rate = 0.9998;   // recalculada en solve() segun max_iters
        double T_END_RATIO = 0.002;     // T_final / T_inicial deseado
        double START_TEMP_FRAC = 0.10;  // un empeoramiento del 10% de d0 se acepta con p=0.5
        double P_VEH_INCREASE = 0.02;   // prob. inicial de aceptar +1 vehiculo (se apaga con T)

        // ---------------- Q-Learning ----------------
        double alpha = 0.05;
        double gamma = 0.8;

        double EPS_DECAY_FRAC = 0.30;   // epsilon llega a su minimo al 30% de la corrida

        int num_states = 2;

        std::vector<std::vector<double>> Q_table_D;
        std::vector<std::vector<double>> Q_table_R;

        // ---------------- Recompensa ----------------
        // La mejora se mide en "puntos": 1 punto = 1% de la distancia inicial,
        // W_VEH puntos = 1 vehiculo. Asi ninguna de las dos escalas anula a la otra.
        double ref_dist = 1.0;          // distancia de la solucion inicial (escala fija)
        double W_VEH = 10.0;            // valor en puntos de un vehiculo
        double eta = 0.6;               // peso de la mejora global vs. la local
        double LAMBDA_WORSE = 0.25;     // atenuacion del castigo por empeorar
        double R_CLIP = 20.0;           // recorte simetrico de la recompensa
        double OC_RATE = 0.005;         // tasa de la EMA del opportunity cost
        double OC_WEIGHT = 0.5;         // fraccion del opportunity cost que se castiga

        // Bonos por evento (misma jerarquia que los scores w1..w4 del ALNS clasico)
        double R_NEW_BEST = 3.0;
        double R_IMPROVED = 1.0;
        double R_ACCEPTED = 0.3;
        double R_REJECTED = -0.3;

        void initOps();
        int selectOp(const std::vector<double>& q_values, double epsilon);

        double objectiveScore(int vehicles, double distance) const;
        double improvement(int ref_veh, double ref_val, const Solution& cand) const;
        Outcome evaluateCandidate(const Solution& cand, double T) const;
};

#endif
