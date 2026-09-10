import os
import csv
import matplotlib.pyplot as plt
import numpy as np

ALGORITMOS = ["CLASSIC", "QLEARNING"]
CLASSES = ["c1", "c2", "r1", "r2", "rc1", "rc2"]

def cargar_metricas_agregadas():
    # Leeremos de los Execution_Results individuales porque tienen TODAS las metricas exactas
    # (Unified_GAP, Veh_GAP, Dist_GAP). La lectura toma menos de 5 milisegundos en Python,
    # asi que no hay penalizacion de velocidad.
    
    datos_globales = {algo: {'veh_gap': [], 'dist_gap': [], 'unified_gap': []} for algo in ALGORITMOS}
    datos_clases = {cls: {algo: {'veh_gap': [], 'dist_gap': [], 'unified_gap': []} for algo in ALGORITMOS} for cls in CLASSES}
    
    for algo in ALGORITMOS:
        filename = f"{algo}_Execution_Results.csv"
        if not os.path.exists(filename):
            continue
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                inst = row['Instancia'].lower().strip()
                
                # Identificar clase
                cls_inst = None
                for c in CLASSES:
                    if inst.startswith(c):
                        cls_inst = c
                        break
                        
                veh_gap = float(row['Veh_GAP'])
                dist_gap = float(row['Dist_GAP'])
                unified_gap = float(row['Unified_GAP'])
                
                datos_globales[algo]['veh_gap'].append(veh_gap)
                datos_globales[algo]['dist_gap'].append(dist_gap)
                datos_globales[algo]['unified_gap'].append(unified_gap)
                
                if cls_inst:
                    datos_clases[cls_inst][algo]['veh_gap'].append(veh_gap)
                    datos_clases[cls_inst][algo]['dist_gap'].append(dist_gap)
                    datos_clases[cls_inst][algo]['unified_gap'].append(unified_gap)
                        
    return datos_globales, datos_clases

def graficar_boxplot_global(datos_dict, titulo, ylabel, filename, color):
    data = [datos_dict[algo] for algo in ALGORITMOS if datos_dict[algo]]
    labels = [algo for algo in ALGORITMOS if datos_dict[algo]]
    
    if not data: return
    
    plt.figure(figsize=(10, 6))
    plt.boxplot(data, tick_labels=labels, patch_artist=True, 
                boxprops=dict(facecolor=color, color='black'),
                medianprops=dict(color='red', linewidth=2))
    plt.title(titulo)
    plt.ylabel(ylabel)
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    plt.savefig(filename, bbox_inches='tight', dpi=300)
    plt.close()

def graficar_boxplot_clases(datos_clases, metrica_key, titulo, ylabel, filename, color):
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle(titulo, fontsize=16)
    axes = axes.flatten()
    
    for idx, cls in enumerate(CLASSES):
        ax = axes[idx]
        data = [datos_clases[cls][algo][metrica_key] for algo in ALGORITMOS if datos_clases[cls][algo][metrica_key]]
        labels = [algo for algo in ALGORITMOS if datos_clases[cls][algo][metrica_key]]
        
        if not data: 
            ax.set_visible(False)
            continue
            
        ax.boxplot(data, tick_labels=labels, patch_artist=True, boxprops=dict(facecolor=color))
        ax.set_title(f'Clase {cls.upper()}')
        ax.set_ylabel(ylabel)
        ax.grid(axis='y', linestyle='--', alpha=0.7)
        
    plt.tight_layout()
    plt.savefig(filename, bbox_inches='tight', dpi=300)
    plt.close()

def generar_boxplots(datos_globales, datos_clases):
    os.makedirs("graficos", exist_ok=True)
    
    # 1. Boxplot GLOBAL del GAP Unificado
    graficar_boxplot_global(
        {algo: datos_globales[algo]['unified_gap'] for algo in ALGORITMOS}, 
        'Distribución GLOBAL: GAP Unificado (Menor es mejor)', 
        'Puntos GAP Unificado', 
        'graficos/Boxplot_Global_UnifiedGAP.png', 
        'bisque'
    )
    
    # 2. Boxplot por Clases del GAP Unificado
    graficar_boxplot_clases(
        datos_clases, 
        'unified_gap', 
        'Distribución de GAP Unificado por Clase de Instancia', 
        'Puntos GAP Unificado', 
        'graficos/Boxplot_Clases_UnifiedGAP.png', 
        'bisque'
    )
    
    # 3. Boxplot por Clases de Vehículos (GAP)
    graficar_boxplot_clases(
        datos_clases, 
        'veh_gap', 
        'Distribución de Vehículos Extra (GAP) por Clase de Instancia', 
        'Vehículos Extra', 
        'graficos/Boxplot_Clases_Vehiculos.png', 
        'lightblue'
    )
    
    # 4. Boxplot por Clases de Distancia (% GAP)
    graficar_boxplot_clases(
        datos_clases, 
        'dist_gap', 
        'Distribución de Exceso de Distancia (% GAP) por Clase de Instancia', 
        '% GAP Distancia', 
        'graficos/Boxplot_Clases_Distancia.png', 
        'lightgreen'
    )

    print("\n-> ¡Boxplots de GAPs agregados generados exitosamente en 'graficos/'!")

if __name__ == "__main__":
    print("Analizando los GAPs promedios (Unificados, Vehículos y Distancia)...")
    g, c = cargar_metricas_agregadas()
    generar_boxplots(g, c)
