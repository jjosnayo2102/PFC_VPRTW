#include "alns_qlearning.h"
#include <iostream>
#include <fstream>

extern std::mt19937 rng;

ALNS_QLearning::ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol)
    : inst(_inst), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS_QLearning::initOps() {
    destroy_ops.push_back(randomRemoval);
    destroy_ops.push_back(routeRemoval);
    destroy_ops.push_back([](Solution& sol, int q) { worstRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { shawRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { timeWindowRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { removeSmallestRoute(sol); });

    repair_ops.push_back([](Solution& sol) { greedyInsertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret2Insertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret3Insertion(sol); });
    repair_ops.push_back([](Solution& sol) { pGreedyInsertion(sol); });

    // Producto cartesiano destroy x repair: cada accion del agente es un PAR
    // completo, como las combinaciones a_1..a_3 del .md.
    for (int d = 0; d < static_cast<int>(destroy_ops.size()); ++d)
        for (int r = 0; r < static_cast<int>(repair_ops.size()); ++r)
            actions.push_back({d, r});

    Q_table.assign(num_states, std::vector<double>(actions.size(), 0.0));
}

// Compresion logaritmica de una mejora relativa (ver nota de escala en el .h).
double ALNS_QLearning::shapeImprovement(double relative_gain) {
    if (relative_gain <= 0.0) return 0.0;
    return std::log1p(reward_gain * relative_gain);
}

// Politica epsilon-greedy: accion aleatoria con probabilidad epsilon
// (exploracion), argmax_a Q(s,a) con probabilidad 1 - epsilon (explotacion).
int ALNS_QLearning::selectAction(int state, double epsilon) {
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < epsilon) {
        std::uniform_int_distribution<int> act_distr(0, actions.size() - 1);
        return act_distr(rng);
    }

    // Explotacion con desempate aleatorio: al inicio todas las celdas valen 0 y
    // un argmax que devuelve siempre el primer maximo sesgaria la politica
    // hacia el par de indice 0 en vez de repartir entre los empatados.
    const std::vector<double>& q_values = Q_table[state];
    double max_q = *std::max_element(q_values.begin(), q_values.end());

    std::vector<int> ties;
    for (int a = 0; a < static_cast<int>(q_values.size()); ++a)
        if (q_values[a] >= max_q - 1e-12) ties.push_back(a);

    std::uniform_int_distribution<int> tie_distr(0, static_cast<int>(ties.size()) - 1);
    return ties[tie_distr(rng)];
}

// Identico al criterio SA de ALNS clasico (misma cost(), mismo tau, mismo
// enfriamiento): la unica diferencia entre ambos algoritmos debe ser el
// criterio de seleccion de operadores, no el de aceptacion.
bool ALNS_QLearning::accept(double cand_cost, double curr_cost, double current_temp) {
    double delta = cand_cost - curr_cost;
    if (delta <= 0) return true;

    double prob = std::exp(-delta / current_temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    return distr(rng) < prob;
}

Solution ALNS_QLearning::solve(int max_iters, bool save_metrics) {
    // La construccion NN puede dejar clientes sin asignar; si arrancamos con
    // best_sol incompleta, cost() la ve mas barata de lo que es (no penaliza
    // unassigned) y ninguna solucion completa logra superarla nunca.
    if (!current_sol.unassigned.empty()) {
        regret2Insertion(current_sol);
        best_sol = current_sol;
    }

    double initial_d = current_sol.total_distance;
    start_temp = -(tau * initial_d) / std::log(0.5);
    double T = start_temp;

    int n_customers = inst.clients.size() - 1;

    // Mismo grado de destruccion que ALNS clasico (misma base).
    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    // --- Calendario de exploracion, reescalado al tamanio real de |A| ---
    // learning_loop: iteraciones iniciales de seleccion puramente aleatoria
    // para poblar la matriz Q antes de empezar a explotarla.
    const int num_actions = static_cast<int>(actions.size());
    const int learning_loop =
        static_cast<int>(std::llround(static_cast<double>(md_learning_loop) *
                                      num_actions / md_num_actions));

    // Ventana de decaimiento: con beta = 0.99 el .md tarda
    // log(eps_min)/log(beta) pasos en llegar al piso; escalamos esa ventana
    // por accion y derivamos el beta equivalente para nuestro |A|.
    const double md_decay_steps = std::log(epsilon_min / epsilon_0) / std::log(md_beta);
    const double decay_steps = md_decay_steps * num_actions / md_num_actions;
    const double epsilon_decay = std::pow(epsilon_min / epsilon_0, 1.0 / decay_steps);

    double epsilon = epsilon_0;

    // Constante de normalizacion de la recompensa: r = delta * e / C. Con
    // C = max_iters el factor e/C queda en (0, 1] y la recompensa es
    // adimensional, de modo que las mejoras logradas tarde en la corrida
    // (mas dificiles) pesan mas que las mismas mejoras al principio.
    const double C = static_cast<double>(max_iters);

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    int current_state = 0;
    int iter = 0;

    while (iter < max_iters) {
        // ================= BUCLE INTERNO: busqueda local =================
        int no_improve = 0;

        while (no_improve < no_improve_limit && iter < max_iters) {
            ++iter;

            Solution candidate = current_sol;
            int q = q_distr(rng);

            // Unico punto de diferencia real con ALNS clasico: el PAR de
            // operadores se elige por epsilon-greedy sobre la tabla Q en vez de
            // por dos ruletas independientes sobre pesos.
            int a_idx = (iter <= learning_loop)
                            ? std::uniform_int_distribution<int>(0, num_actions - 1)(rng)
                            : selectAction(current_state, epsilon);

            const OpPair& action = actions[a_idx];
            destroy_ops[action.destroy_idx](candidate, q);
            repair_ops[action.repair_idx](candidate);

            // ---------------- RECOMPENSA BASADA EN VALOR ----------------
            // Reemplaza los scores categoricos {45,33,13,9,0} de la version
            // anterior. Esos scores eran los MISMOS que usa la ruleta del ALNS
            // clasico, asi que la tabla Q terminaba estimando exactamente la
            // misma cantidad que los pesos de la ruleta (el score esperado por
            // operador): misma informacion => mismo comportamiento, que es por
            // que ambos metodos daban NV identica instancia por instancia.
            //
            // La recompensa por valor mide la MAGNITUD relativa de la mejora,
            // no su categoria. Ademas codifica sola la jerarquia (NV antes que
            // TD): como cost() = NV*10000 + TD, quitar un vehiculo vale
            // ~10000/cost ~ 8e-2 de mejora relativa mientras que recortar unas
            // unidades de distancia vale ~1e-5. La prioridad queda expresada
            // en la propia escala de la recompensa y ya no hace falta el score
            // artificial w0 para vehiculos.
            double delta_global = 0.0;
            double delta_local = 0.0;

            // cost() no penaliza clientes sin asignar (eso solo lo hace
            // cost_phase1). Si el repair no logro reinsertar a todos, la
            // solucion es infactible y no debe competir por costo: se trata
            // igual que cualquier rechazo (mejora nula => penalizacion por
            // costo de oportunidad).
            if (candidate.unassigned.empty()) {
                double cand_cost = cost(candidate);

                // Ambas mejoras se acotan en 0: son eventos de progreso. Un
                // candidato peor no aporta magnitud negativa, cae en la rama de
                // "sin mejora" y recibe la penalizacion por costo de
                // oportunidad, que es lo que describe el .md.
                delta_global = std::max(0.0, (best_cost - cand_cost) / best_cost);
                delta_local = std::max(0.0, (curr_cost - cand_cost) / curr_cost);

                if (cand_cost < best_cost) {
                    best_sol = candidate;
                    current_sol = candidate;
                    curr_cost = cand_cost;
                    best_cost = cand_cost;
                } else if (cand_cost < curr_cost) {
                    current_sol = candidate;
                    curr_cost = cand_cost;
                } else if (accept(cand_cost, curr_cost, T)) {
                    current_sol = candidate;
                    curr_cost = cand_cost;
                }
                // else: rechazada
            }

            // delta_improvement = delta_global * eta + delta_local * (1 - eta),
            // con la compresion log1p aplicada a cada termino (ver .h).
            double delta_improvement = shapeImprovement(delta_global) * eta
                                     + shapeImprovement(delta_local) * (1.0 - eta);
            bool improvement = delta_improvement > 0.0;

            double reward;
            if (improvement) {
                // r = delta * e / C, y se actualiza el costo de oportunidad
                // (maximo valor de mejora registrado).
                opportunity_cost = std::max(opportunity_cost, delta_improvement);
                reward = delta_improvement * iter / C;
            } else {
                // r = (delta - OC) * e / C: penalizacion por costo de
                // oportunidad, lo que se dejo de ganar en esta iteracion.
                reward = (delta_improvement - opportunity_cost) * iter / C;
            }

            // Estado binario: 1 si la busqueda local mejoro (global o local),
            // 0 si no hubo mejora (estancamiento en optimo local).
            int next_state = improvement ? 1 : 0;

            double max_next_q = *std::max_element(Q_table[next_state].begin(),
                                                  Q_table[next_state].end());

            // Q(s,a) <- Q(s,a) + alpha * (r + gamma * max_a' Q(s',a') - Q(s,a))
            Q_table[current_state][a_idx] +=
                alpha * (reward + gamma * max_next_q - Q_table[current_state][a_idx]);

            current_state = next_state;

            T = T * cooling_rate;
            if (iter > learning_loop)
                epsilon = std::max(epsilon_min, epsilon * epsilon_decay);

            no_improve = improvement ? 0 : (no_improve + 1);
        }

        if (iter >= max_iters) break;

        // ================= BUCLE EXTERNO: perturbacion =================
        // Identico al del ALNS clasico (pertenece a la base ALNS del .md,
        // seccion 1, no a la capa de Q-learning).
        current_sol = best_sol;
        perturbRouteElimination(current_sol, perturb_k);
        regret2Insertion(current_sol);

        if (!current_sol.unassigned.empty()) {
            // La reinsercion no cerro; no arrastramos una solucion incompleta.
            current_sol = best_sol;
        }

        curr_cost = cost(current_sol);
        if (curr_cost < best_cost) {
            best_sol = current_sol;
            best_cost = curr_cost;
        }
    }

    return best_sol;
}
