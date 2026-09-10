import os
import csv
import matplotlib.pyplot as plt
import numpy as np

ALGORITMOS = ["CLASSIC", "QLEARNING"]
CLASSES = ["c1", "c2", "r1", "r2", "rc1", "rc2"]

def cargar_resultados():
    datos = {algo: {} for algo in ALGORITMOS}
    instancias_comunes = set()
    
    for algo in ALGORITMOS:
        filename = f"{algo}_Execution_Results.csv"
        if not os.path.exists(filename):
            print(f"[ERROR] No se encontro {filename}. Ejecuta automate.py primero.")
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                inst = row['Instancia'].lower().strip()
                if inst:
                    datos[algo][inst] = {
                        'opt_veh': float(row['Opt_Veh']),
                        'opt_dist': float(row['Opt_Dist']),
                        'best_veh': float(row['Best_Veh']),
                        'avg_veh': float(row['Avg_Veh']),
                        'std_veh': float(row['Std_Veh']),
                        'best_dist': float(row['Best_Dist']),
                        'avg_dist': float(row['Avg_Dist']),
                        'std_dist': float(row['Std_Dist']),
                        'veh_gap': float(row['Veh_GAP']),
                        'dist_gap': float(row['Dist_GAP']),
                        'unified_gap': float(row['Unified_GAP']),
                        'avg_time': float(row.get('Avg_Time(s)', 0.0)),
                        'hits': 1 if float(row['Avg_Veh']) == float(row['Opt_Veh']) else 0
                    }
                    if algo == ALGORITMOS[0]:
                        instancias_comunes.add(inst)
                        
    # Intersect
    for algo in ALGORITMOS:
        if datos[algo]:
            instancias_comunes &= set(datos[algo].keys())
            
    return datos, sorted(list(instancias_comunes))

def imprimir_resumen_terminal(datos, instancias_comunes):
    total_inst = len(instancias_comunes)
    if total_inst == 0:
        return
        
    print("\n" + "="*85)
    print(" VEREDICTO FINAL: ALNS CLÁSICO vs ALNS Q-LEARNING")
    print("="*85)
    print(f"Instancias evaluadas en conjunto: {total_inst}")
    print("-" * 85)
    
    for name in ALGORITMOS:
        data = datos[name]
        if not data: continue
        
        avg_veh_gap = sum(data[inst]['veh_gap'] for inst in instancias_comunes) / total_inst
        avg_dist_gap = sum(data[inst]['dist_gap'] for inst in instancias_comunes) / total_inst
        avg_unif_gap = sum(data[inst]['unified_gap'] for inst in instancias_comunes) / total_inst
        avg_time = sum(data[inst]['avg_time'] for inst in instancias_comunes) / total_inst
        avg_std_veh = sum(data[inst]['std_veh'] for inst in instancias_comunes) / total_inst
        avg_std_dist = sum(data[inst]['std_dist'] for inst in instancias_comunes) / total_inst
        hits = sum(data[inst]['hits'] for inst in instancias_comunes)
        
        print(f"[{name}]")
        print(f"  - Hits de Vehículo Optimo:                    {hits}/{total_inst} ({hits/total_inst*100:.1f}%)")
        print(f"  - Castigo de Vehículos (Media Avg_Veh_GAP):   +{avg_veh_gap:.3f} vehículos extras")
        print(f"  - Castigo de Distancia (Media Avg_Dist_GAP):  {avg_dist_gap:.2f} % de exceso")
        print(f"  - Variación Promedio Vehículos (Std_Veh):     {avg_std_veh:.3f}")
        print(f"  - Variación Promedio Distancia (Std_Dist):    {avg_std_dist:.2f}")
        print(f"  - Tiempo Promedio de Ejecución:               {avg_time:.2f} s")
        print(f"  - GAP UNIFICADO GLOBAL (Promedio):            {avg_unif_gap:.2f} Puntos (Menor es mejor)")
        print("-" * 85)

def exportar_csv_global(datos, instancias_comunes):
    filename = "GLOBAL_Comparison_Summary.csv"
    with open(filename, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Headers
        headers = ['Instancia', 'Opt_Veh', 'Opt_Dist']
        for algo in ALGORITMOS:
            headers.extend([
                f'{algo}_BestVeh', f'{algo}_StdVeh',
                f'{algo}_BestDist', f'{algo}_StdDist',
                f'{algo}_AvgTime', f'{algo}_UnifiedGAP'
            ])
        writer.writerow(headers)
        
        for inst in instancias_comunes:
            # Los optimos son los mismos para todos
            opt_veh = datos[ALGORITMOS[0]][inst]['opt_veh']
            opt_dist = datos[ALGORITMOS[0]][inst]['opt_dist']
            
            row = [inst.upper(), opt_veh, opt_dist]
            for algo in ALGORITMOS:
                d = datos[algo][inst]
                row.extend([
                    d['best_veh'], f"{d['std_veh']:.3f}",
                    f"{d['best_dist']:.2f}", f"{d['std_dist']:.2f}",
                    f"{d['avg_time']:.2f}", f"{d['unified_gap']:.2f}"
                ])
            writer.writerow(row)
            
    print(f"-> Exportado resumen comparativo global: {filename}")

def generar_graficos(datos_agregados, instancias_comunes):
    os.makedirs("graficos", exist_ok=True)
    total_inst = len(instancias_comunes)
    if total_inst == 0: return
    
    # Calcular resumen para graficos
    resumen_graficos = {algo: {} for algo in ALGORITMOS}
    for name in ALGORITMOS:
        data = datos_agregados[name]
        avg_veh_gap = sum(data[inst]['veh_gap'] for inst in instancias_comunes) / total_inst
        avg_dist_gap = sum(data[inst]['dist_gap'] for inst in instancias_comunes) / total_inst
        avg_unif_gap = sum(data[inst]['unified_gap'] for inst in instancias_comunes) / total_inst
        resumen_graficos[name] = {
            'avg_veh_gap': avg_veh_gap,
            'avg_dist_gap': avg_dist_gap,
            'avg_unif_gap': avg_unif_gap
        }
        
    # 1. Gráfico Combinado (GAPs)
    labels = ALGORITMOS
    x = np.arange(len(labels))
    width = 0.25
    
    veh_gaps = [resumen_graficos[algo]['avg_veh_gap'] for algo in ALGORITMOS]
    dist_gaps = [resumen_graficos[algo]['avg_dist_gap'] for algo in ALGORITMOS]
    unif_gaps = [resumen_graficos[algo]['avg_unif_gap'] for algo in ALGORITMOS]
    
    fig, ax1 = plt.subplots(figsize=(10, 6))
    
    rects1 = ax1.bar(x - width, veh_gaps, width, label='Vehicles GAP (+x extras)', color='lightblue')
    rects2 = ax1.bar(x, dist_gaps, width, label='Distance GAP (%)', color='lightgreen')
    
    ax2 = ax1.twinx()
    rects3 = ax2.bar(x + width, unif_gaps, width, label='Unified GAP (Score)', color='salmon')
    
    ax1.set_ylabel('GAPs Físicos (Vehículos / % Distancia)')
    ax2.set_ylabel('GAP Unificado (Score)', color='darkred')
    ax1.set_title('Comparación de GAPs por Algoritmo (Menor es mejor)')
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    
    lines, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax2.legend(lines + lines2, labels1 + labels2, loc='upper left')
    
    plt.savefig('graficos/Comparacion_GAP_General.png', bbox_inches='tight', dpi=300)
    plt.close()
    
    # 2. Gráficos Individuales por Clase
    for cls in CLASSES:
        insts_cls = [inst for inst in instancias_comunes if inst.startswith(cls)]
        if not insts_cls: continue
        
        x_inst = np.arange(len(insts_cls))
        width_cls = 0.25
        
        fig, ax = plt.subplots(figsize=(max(12, len(insts_cls)*1.2), 7))
        
        for i, algo in enumerate(ALGORITMOS):
            best_dists = [datos_agregados[algo][inst]['best_dist'] for inst in insts_cls]
            best_vehs = [datos_agregados[algo][inst]['best_veh'] for inst in insts_cls]
            opt_vehs = [datos_agregados[algo][inst]['opt_veh'] for inst in insts_cls]
            
            offset = (i - 1) * width_cls
            bars = ax.bar(x_inst + offset, best_dists, width_cls, label=algo, alpha=0.8)
            
            for bar, veh, opt_v in zip(bars, best_vehs, opt_vehs):
                height = bar.get_height()
                color = 'black' if veh == opt_v else 'red'
                weight = 'normal' if veh == opt_v else 'bold'
                ax.annotate(f'v={int(veh)}',
                            xy=(bar.get_x() + bar.get_width() / 2, height),
                            xytext=(0, 4),  
                            textcoords="offset points",
                            ha='center', va='bottom', fontsize=9, color=color, weight=weight, rotation=90)
            
        opt_dists = [datos_agregados[ALGORITMOS[0]][inst]['opt_dist'] for inst in insts_cls]
        ax.plot(x_inst, opt_dists, 'kX', markersize=8, label='Distancia Óptima (SINTEF)', zorder=5)
            
        ax.set_ylabel('Mejor Distancia Alcanzada')
        ax.set_title(f'Resultados Clase {cls.upper()}: Distancia y Vehículos (v=X)\nTextos en ROJO indican que no se alcanzó el mínimo óptimo')
        ax.set_xticks(x_inst)
        ax.set_xticklabels([i.upper() for i in insts_cls], rotation=45)
        
        ymax = max([datos_agregados[algo][inst]['best_dist'] for inst in insts_cls for algo in ALGORITMOS])
        ax.set_ylim(0, ymax * 1.25)
        ax.legend(loc='upper right')
        
        plt.tight_layout()
        plt.savefig(f'graficos/Comparacion_Instancias_{cls.upper()}.png', bbox_inches='tight', dpi=300)
        plt.close()
        
    print("-> Gráficos generados exitosamente en la subcarpeta 'graficos/'.")

if __name__ == "__main__":
    datos, comunes = cargar_resultados()
    if comunes:
        imprimir_resumen_terminal(datos, comunes)
        exportar_csv_global(datos, comunes)
        generar_graficos(datos, comunes)
