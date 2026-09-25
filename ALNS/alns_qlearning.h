#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include "../Operators/operators.h"

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters, bool save_metrics = false);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;
        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        // ===================================================================
        // ESPACIO DE ACCIONES: PARES (destroy, repair)
        // ===================================================================
        // El .md define A como el conjunto de COMBINACIONES destroy&repair
        // (a_1 = rRd&iFwd, a_2 = rMxM&iUp, a_3 = rRd&iBtwInsert): cada accion
        // es un par completo, no un operador suelto.
        //
        // La version anterior mantenia dos tablas Q independientes (una para
        // destroy y otra para repair) y las actualizaba con la MISMA
        // recompensa. Eso rompe la asignacion de credito: si worstRemoval +
        // regret3 funciona bien, la actualizacion premia tambien a worstRemoval
        // combinado con greedy, que puede ser malo. Aprendiendo sobre el par se
        // captura la sinergia destroy-repair, que es justo lo que la ruleta del
        // ALNS clasico (dos ruletas independientes) NO puede representar. Esta
        // es la diferencia estructural que permite que el hibrido supere al
        // clasico en vez de replicarlo.
        struct OpPair {
            int destroy_idx;
            int repair_idx;
        };
        std::vector<OpPair> actions;

        // ===================================================================
        // BASE ALNS COMPARTIDA CON EL ALNS CLASICO (.md seccion 1)
        // ===================================================================
        // Identica a la de ALNS: misma cost(), mismo SA, mismo grado de
        // destruccion, mismo two-layer loop. La UNICA diferencia entre ambos
        // metodos es la capa de seleccion de operadores (AOS).
        static constexpr double tau = 0.2; // escala de temperatura inicial
        double start_temp;
        double cooling_rate = 0.9995;

        static const int no_improve_limit = 20; // t: limite de no mejora del bucle interno
        static const int perturb_k = 3;

        // ===================================================================
        // HIPERPARAMETROS DE Q-LEARNING (.md seccion 3, valores sintonizados
        // por RSM + Box-Behnken)
        // ===================================================================
        static constexpr double alpha = 0.5;        // tasa de aprendizaje
        static constexpr double gamma = 0.7;        // factor de descuento
        static constexpr double epsilon_0 = 1.0;    // exploracion inicial
        static constexpr double epsilon_min = 0.05; // piso de exploracion
        static constexpr double eta = 0.8;          // peso de la mejora global en la recompensa

        // El .md ajusta l = 200 y beta = 0.99 sobre un espacio de acciones ya
        // reducido a |A| = 3 por el experimento de filtrado de operadores (OF,
        // dominancia de Wilcoxon al 40%). Aqui |A| = 24 pares sin filtrar, asi
        // que esos valores absolutos dejarian ~8 muestras por par antes de
        // pasar a explotacion pura y la tabla Q quedaria practicamente sin
        // aprender. Se conservan los valores del .md expresados como MUESTRAS
        // POR ACCION, de modo que el ajuste coincide exactamente con el .md
        // cuando |A| = 3 y escala correctamente con cualquier otro tamanio del
        // espacio de acciones. Si mas adelante se corre el experimento OF y
        // |A| baja a 3, estos valores reproducen literalmente l = 200 y
        // beta = 0.99.
        static const int md_num_actions = 3;
        static const int md_learning_loop = 200;    // l del .md
        static constexpr double md_beta = 0.99;     // decaimiento del .md

        // ===================================================================
        // ESPACIO DE ESTADOS BINARIO (.md seccion 2): S = {0, 1}
        // ===================================================================
        // s' = 1: la busqueda local de la iteracion reciente logro una mejor
        //         solucion (mejora global o local).
        // s' = 0: no hubo ninguna mejora (senial de estancamiento).
        static const int num_states = 2;
        std::vector<std::vector<double>> Q_table;

        // Costo de oportunidad: mayor valor de mejora registrado hasta ahora.
        // Es la penalizacion que se aplica cuando una iteracion no mejora nada,
        // es decir, lo que se dejo de ganar por haber gastado esa iteracion.
        double opportunity_cost = 0.0;

        // --- Escala de la recompensa (adaptacion a VRPTW) ------------------
        // Con cost() = 10000*NV + TD la mejora relativa abarca ~4 ordenes de
        // magnitud: ~1e-5 para un ajuste de distancia y ~8e-2 para eliminar un
        // vehiculo. Usando la mejora cruda, los (raros) eventos de vehiculo
        // dominan la media por accion y el estimador queda de varianza alta,
        // que es justo lo que hace que un argmax elija ruido. La compresion
        // log1p(gain*delta) preserva el orden -- un vehiculo menos sigue
        // valiendo ~70x una buena mejora de distancia -- pero acota el rango
        // para que la media sea estable.
        static constexpr double reward_gain = 1.0e4;

        void initOps();
        int selectAction(int state, double epsilon);
        bool accept(double cand_cost, double curr_cost, double current_temp);
        static double shapeImprovement(double relative_gain);
};

#endif
