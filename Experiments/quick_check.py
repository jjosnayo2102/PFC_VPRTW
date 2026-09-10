"""
Verificacion rapida (no reemplaza a automate.py).

Corre un subconjunto de instancias con pocas repeticiones y compara el GAP unificado
contra los CSV de resultados ya guardados. Sirve para saber en minutos si un cambio
en el algoritmo va en la direccion correcta antes de lanzar el benchmark completo.

Uso:
    python quick_check.py                      # subconjunto duro por defecto, 3 runs
    python quick_check.py --runs 5
    python quick_check.py --algos QLEARNING
    python quick_check.py --instancias r101,rc101,c101
    python quick_check.py --iters 25000
"""

import argparse
import csv
import os
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from statistics import mean

EXEC_PATH = "../build/Release/ALNS_VRPTW.exe"
BENCHMARK_DIR = "../solomon-100"
SINTEF_CSV = "sintef.csv"

# Instancias donde esta el margen de mejora (R1/RC1 pagan vehiculos extra),
# mas dos controles de clase C que ya estan resueltas al optimo.
SUBSET_DURO = [
    "r101", "r104", "r107", "r110", "r112",
    "rc101", "rc103", "rc105", "rc108",
    "r201", "r207", "rc201", "rc205",
    "c101", "c201",
]


def cargar_optimos():
    optimos = {}
    with open(SINTEF_CSV, "r", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f, delimiter=";"):
            inst = row["Instancia"].lower().strip()
            optimos[inst] = {
                "veh": float(row["Vehículos"]),
                "dist": float(row["Distancia"]),
            }
    if "r110" in optimos and optimos["r110"]["dist"] == 118.84:
        optimos["r110"]["dist"] = 1118.84
    return optimos


def localizar_instancia(nombre):
    for raiz, _, archivos in os.walk(BENCHMARK_DIR):
        for a in archivos:
            if a.lower() == f"{nombre}.txt":
                return os.path.join(raiz, a)
    return None


def cargar_baseline(algo):
    """GAP unificado por instancia del CSV de resultados guardado."""
    fname = f"{algo}_Execution_Results.csv"
    if not os.path.exists(fname):
        return {}
    base = {}
    with open(fname, "r", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            base[row["Instancia"].lower().strip()] = {
                "unified": float(row["Unified_GAP"]),
                "veh_gap": float(row["Veh_GAP"]),
                "dist_gap": float(row["Dist_GAP"]),
            }
    return base


def correr(path, algo, iters):
    try:
        r = subprocess.run(
            [EXEC_PATH, path, algo, str(iters), "BENCHMARK"],
            capture_output=True, text=True, check=True,
        )
        m = re.search(r"\[FINAL_RESULT\] Veh: (\d+), Dist: ([\d.]+)", r.stdout)
        if m:
            return int(m.group(1)), float(m.group(2))
    except subprocess.CalledProcessError:
        pass
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--iters", type=int, default=25000)
    ap.add_argument("--algos", default="CLASSIC,QLEARNING")
    ap.add_argument("--instancias", default=",".join(SUBSET_DURO))
    args = ap.parse_args()

    algos = [a.strip().upper() for a in args.algos.split(",") if a.strip()]
    nombres = [i.strip().lower() for i in args.instancias.split(",") if i.strip()]

    optimos = cargar_optimos()
    rutas = {}
    for n in nombres:
        p = localizar_instancia(n)
        if p is None or n not in optimos:
            print(f"[Aviso] instancia ignorada: {n}")
            continue
        rutas[n] = p

    tareas = [(n, a, k) for n in rutas for a in algos for k in range(args.runs)]
    resultados = {a: {n: [] for n in rutas} for a in algos}

    print(f"=== {len(rutas)} instancias | {algos} | {args.runs} runs | {args.iters} iters ===")
    t0 = time.time()

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        futuros = {ex.submit(correr, rutas[n], a, args.iters): (n, a) for (n, a, _) in tareas}
        hechas = 0
        for fut in futuros:
            n, a = futuros[fut]
            res = fut.result()
            hechas += 1
            if res:
                resultados[a][n].append(res)
            print(f"[{hechas}/{len(tareas)}] {a} {n.upper()} -> {res}", flush=True)

    print(f"\n--- Resultados ({time.time() - t0:.0f}s) ---")
    baselines = {a: cargar_baseline(a) for a in algos}

    for a in algos:
        print(f"\n[{a}]")
        print(f"{'inst':8} {'veh':>5} {'dist':>9} {'uniGAP':>8} {'base':>8} {'delta':>8}")
        actuales, previos = [], []
        for n in rutas:
            runs = resultados[a][n]
            if not runs:
                print(f"{n.upper():8} {'ERROR':>5}")
                continue
            avg_veh = mean([r[0] for r in runs])
            avg_dist = mean([r[1] for r in runs])
            veh_gap = avg_veh - optimos[n]["veh"]
            dist_gap = (avg_dist - optimos[n]["dist"]) / optimos[n]["dist"] * 100
            uni = veh_gap * 100 + dist_gap
            base = baselines[a].get(n, {}).get("unified")
            actuales.append(uni)
            if base is not None:
                previos.append(base)
                delta = uni - base
                marca = "" if abs(delta) < 1 else (" <<" if delta < 0 else " >>")
                print(f"{n.upper():8} {avg_veh:5.1f} {avg_dist:9.1f} {uni:8.2f} {base:8.2f} {delta:+8.2f}{marca}")
            else:
                print(f"{n.upper():8} {avg_veh:5.1f} {avg_dist:9.1f} {uni:8.2f} {'-':>8} {'-':>8}")

        if actuales:
            print(f"{'MEDIA':8} {'':5} {'':9} {mean(actuales):8.2f}", end="")
            if len(previos) == len(actuales):
                print(f" {mean(previos):8.2f} {mean(actuales) - mean(previos):+8.2f}")
            else:
                print()

    print("\n'base' = Unified_GAP guardado en los CSV (10 runs). '<<' mejora, '>>' empeora.")
    print("Ojo: con pocos runs el ruido es alto; usa automate.py para el veredicto final.")


if __name__ == "__main__":
    main()
