import os
import subprocess
import time
import glob
import csv
from concurrent.futures import ThreadPoolExecutor, as_completed

EXEC_PATH = "../build/ALNS_vrptw.exe"
BENCHMARK_DIR = "../solomon-100"
RESULTS_DIR = "../Results"

ALGORITMOS = ["CLASSIC", "QLEARNING"]
ITERACIONES = 25000
RUNS = 10
MAX_WORKERS = os.cpu_count() or 4  # Ajusta al número de hilos de tu CPU

# Variable para excluir instancias C1 y C2 (ya que suelen alcanzar el óptimo rápido)
# Ponlo en False si quieres volver a correr TODAS las instancias.
EXCLUIR_C1_C2 = True

def preparar_directorios():
    print("Verificando estructura de directorios...")
    for algo in ALGORITMOS:
        metrics_dir = os.path.join(RESULTS_DIR, algo, "metrics")
        routes_dir = os.path.join(RESULTS_DIR, algo, "routes")
        os.makedirs(metrics_dir, exist_ok=True)
        os.makedirs(routes_dir, exist_ok=True)
        print(f"  -> Creado/Verificado: {metrics_dir}")
        print(f"  -> Creado/Verificado: {routes_dir}")

def obtener_instancias():
    patron = os.path.join(BENCHMARK_DIR, "**", "*.txt")
    instancias = glob.glob(patron, recursive=True)
    instancias_filtradas = []
    
    for inst in instancias:
        nombre = os.path.basename(inst).lower()
        if EXCLUIR_C1_C2 and (nombre.startswith('c1') or nombre.startswith('c2')):
            continue
        instancias_filtradas.append(inst)
        
    return sorted(instancias_filtradas)

def ejecutar_corrida(inst_path, algo, run):
    inst_name = os.path.basename(inst_path).replace('.txt', '')
    comando = [
        EXEC_PATH,
        inst_path,       
        algo,            
        str(ITERACIONES),
        str(run)         
    ]
    start_run = time.perf_counter()
    veh = 0
    dist = 0.0
    try:
        resultado = subprocess.run(comando, capture_output=True, text=True, check=True)
        end_run = time.perf_counter()
        time_run = end_run - start_run
        
        import re
        match = re.search(r'\[FINAL_RESULT\] Veh: (\d+), Dist: ([\d.]+)', resultado.stdout)
        if match:
            veh = int(match.group(1))
            dist = float(match.group(2))
            
        return {
            "Instancia": inst_name.lower(),
            "Algoritmo": algo,
            "Run": run,
            "Tiempo_s": round(time_run, 4),
            "Veh": veh,
            "Dist": dist,
            "Status": "OK"
        }
    except subprocess.CalledProcessError as e:
        return {
            "Instancia": inst_name.lower(),
            "Algoritmo": algo,
            "Run": run,
            "Tiempo_s": 0.0,
            "Veh": 0,
            "Dist": 0.0,
            "Status": "ERROR",
            "ErrorMsg": e.stderr
        }

def ejecutar_experimentos():
    preparar_directorios()
    instancias = obtener_instancias()
    if not instancias:
        print(f"[ERROR] No se encontraron instancias en {BENCHMARK_DIR}")
        return

    total_instancias = len(instancias)
    total_runs = total_instancias * len(ALGORITMOS) * RUNS
    
    print(f"\n=== Iniciando experimentos PARALELOS ({MAX_WORKERS} hilos): {total_instancias} instancias | {total_runs} ejecuciones ===")
    start_total = time.time()
    
    tareas = []
    for inst_path in instancias:
        for algo in ALGORITMOS:
            for run in range(1, RUNS + 1):
                tareas.append((inst_path, algo, run))
                
    registro_tiempos = []
    completadas = 0

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futuros = {executor.submit(ejecutar_corrida, inst, algo, run): (inst, algo, run) for (inst, algo, run) in tareas}
        
        for futuro in as_completed(futuros):
            res = futuro.result()
            completadas += 1
            if res["Status"] == "OK":
                print(f"[{completadas}/{total_runs}] {res['Algoritmo']} | {res['Instancia'].upper()} | Run {res['Run']} [OK] ({res['Tiempo_s']}s) - Veh: {res['Veh']}, Dist: {res['Dist']}")
                registro_tiempos.append({
                    "Instancia": res["Instancia"],
                    "Algoritmo": res["Algoritmo"],
                    "Run": res["Run"],
                    "Tiempo_s": res["Tiempo_s"],
                    "Veh": res["Veh"],
                    "Dist": res["Dist"]
                })
            else:
                print(f"[{completadas}/{total_runs}] {res['Algoritmo']} | {res['Instancia'].upper()} | Run {res['Run']} [ERROR FATAL]")
                
    end_total = time.time()

    csv_path = os.path.join(RESULTS_DIR, "tiempos_ejecucion_limpios.csv")
    with open(csv_path, mode='w', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=["Instancia", "Algoritmo", "Run", "Tiempo_s", "Veh", "Dist"])
        writer.writeheader()
        writer.writerows(registro_tiempos)

    horas, rem = divmod(end_total - start_total, 3600)
    minutos, segundos = divmod(rem, 60)
    print(f"\n=== Experimentos paralelos terminados en {int(horas)}h {int(minutos)}m {segundos:.2f}s ===")
    print(f"-> Archivo de tiempos guardado con éxito en: {csv_path}")

if __name__ == "__main__":
    ejecutar_experimentos()