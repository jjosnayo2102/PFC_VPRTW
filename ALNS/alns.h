#ifndef ALNS_H
#define ALNS_H

#include <vector>
#include <functional>
#include <string>
#include "../Operators/operators.h"

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS {
    public:
        ALNS(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;

        // Operadores de destroy (Omega^-)
        std::vector<DestroyOp> destroy_ops;
        std::vector<double> destroy_weights;

        // Operadores de repair (Omega^+)
        std::vector<RepairOp> repair_ops;
        std::vector<double> repair_weights;

        // Hiperparametros del ALNS
        double decay = 0.9;
        double w1 = 33.0;
        double w2 = 13.0;
        double w3 = 9.0;
        double w4 = 0.0;

        // Actualizacion de pesos por segmento (Ropke & Pisinger): los scores se
        // acumulan durante 'segment_size' iteraciones y los pesos se actualizan
        // una sola vez al cierre del segmento, con el promedio del segmento.
        static const int segment_size = 100;
        std::vector<double> destroy_scores;
        std::vector<int> destroy_uses;
        std::vector<double> repair_scores;
        std::vector<int> repair_uses;

        // Hiperparametros del Simulated Annealing.
        // tau: escala de temperatura inicial, T0 = -(tau * d0)/log(0.5).
        // Valor sintonizado por RSM/Box-Behnken en el .md (seccion 3).
        static constexpr double tau = 0.2;
        double start_temp;
        double cooling_rate = 0.9995;

        // Two-layer loop (.md seccion 1). Este es el esqueleto del ALNS BASE y
        // por tanto es comun a ALNS y ALNS_QLearning: la unica diferencia entre
        // ambos metodos sigue siendo la capa de seleccion de operadores (AOS).
        //   - Bucle interno: busqueda local con pares destroy/repair bajo la
        //     configuracion de rutas vigente, hasta acumular 'no_improve_limit'
        //     iteraciones consecutivas sin mejora.
        //   - Bucle externo: perturbacion estructural sobre el mejor global.
        // Sin esto ambos metodos eran un unico bucle plano sin ningun mecanismo
        // de diversificacion, y quedaban clavados en el NV que les daba la
        // solucion inicial (misma NV exacta en los dos, en toda instancia).
        // t: iteraciones consecutivas sin mejora que agotan el bucle interno.
        // El .md sintoniza t = 20, pero ahi el bucle interno es una busqueda
        // local fina sobre la secuencia de camiones. Aqui un solo destroy
        // elimina entre 10 y 40 de los 100 clientes, asi que la mayoria de las
        // iteraciones no mejora y t = 20 se agota cada ~25 iteraciones: el
        // bucle interno nunca alcanza a descender y el SA queda anulado.
        // Medido sobre 10 instancias R/RC (3 semillas pareadas, 25k
        // iteraciones), t = 750 frente a t = 20 deja el gap de NV del hibrido
        // en 7.83% vs 12.83% y lo pone 5-2 arriba del clasico en comparacion
        // directa. Se expresa como fraccion del presupuesto para que escale con
        // max_iters (con 25000 iteraciones da exactamente los 750 medidos) y se
        // acota por abajo con el valor del .md.
        static constexpr double no_improve_fraction = 0.03;
        static const int md_no_improve_limit = 20;
        static const int perturb_k = 3;

        void initOps();
        int selectDestroyOp();
        int selectRepairOp();
        bool accept(double cand_cost, double curr_cost, double current_temp);
        void registerScore(int used_destroy_idx, int used_repair_idx, double score);
        void updateWeightsSegment();
};

#endif //ALNS_H