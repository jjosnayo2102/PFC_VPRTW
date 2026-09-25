import os
import sys
import pandas as pd

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ANALISIS_DIR = os.path.join(BASE_DIR, "Analisis")
TABLAS_DIR = os.path.join(ANALISIS_DIR, "Tablas")
os.makedirs(TABLAS_DIR, exist_ok=True)

# Ponderación del GAP Unificado: debe ser idéntica a VEHICLE_COST en
# "VRPTW Environment/solution.cpp" (función cost()).
VEHICLE_WEIGHT = 10000

class Logger:
    def __init__(self, filename):
        self.terminal = sys.stdout
        self.log = open(filename, "w", encoding='utf-8')
    def write(self, message):
        self.terminal.write(message)
        self.log.write(message)
    def flush(self):
        self.terminal.flush()
        self.log.flush()

sys.stdout = Logger(os.path.join(ANALISIS_DIR, "conclusiones_comparativas.txt"))

print("=== INICIO DEL ANÁLISIS DE RESULTADOS ===")
print(f"""
Fórmula del GAP Unificado (misma ponderación que cost() en el solver C++):
  Cost(NV, TD)       = {VEHICLE_WEIGHT} * NV + TD
  GAP_Unificado(%)   = (Cost_Nuestro - Cost_BKS) / Cost_BKS * 100
Es decir: un vehículo adicional equivale a {VEHICLE_WEIGHT} unidades de distancia.
""")

df_res = pd.read_csv(os.path.join(BASE_DIR, "resultados_ejecuciones_iterativas.csv"))
df_sintef = pd.read_csv(os.path.join(BASE_DIR, "sintef.csv"), sep=";", encoding="latin1")

df_sintef.columns = ["Instancia", "BKS_Vehiculos", "BKS_Distancia"]
df_sintef["Instancia"] = df_sintef["Instancia"].str.lower()
df_res["Instancia"] = df_res["Instancia"].str.lower()

df = pd.merge(df_res, df_sintef, on="Instancia", how="inner")

if len(df) == 0:
    print("Error: No se pudieron cruzar las instancias.")
    sys.exit(1)

def get_class(inst):
    if inst.startswith('rc'): return inst[:3].upper()
    return inst[:2].upper()

df["Clase"] = df["Instancia"].apply(get_class)

df["Cost_Ours"] = df["Avg_Vehiculos"] * VEHICLE_WEIGHT + df["Avg_Distancia"]
df["Cost_BKS"] = df["BKS_Vehiculos"] * VEHICLE_WEIGHT + df["BKS_Distancia"]

df["GAP_Vehiculos(%)"] = ((df["Avg_Vehiculos"] - df["BKS_Vehiculos"]) / df["BKS_Vehiculos"]) * 100
df["GAP_Distancia(%)"] = ((df["Avg_Distancia"] - df["BKS_Distancia"]) / df["BKS_Distancia"]) * 100
df["GAP_Unificado(%)"] = ((df["Cost_Ours"] - df["Cost_BKS"]) / df["Cost_BKS"]) * 100

cols_instancia = ["Instancia", "Clase", "Algoritmo", "Avg_Vehiculos", "Avg_Distancia", 
                  "BKS_Vehiculos", "BKS_Distancia", "GAP_Vehiculos(%)", "GAP_Distancia(%)", "GAP_Unificado(%)"]
df_instancia = df[cols_instancia].copy()
df_instancia.to_csv(os.path.join(TABLAS_DIR, "gap_por_instancia.csv"), index=False)
print(f"-> Generada tabla: gap_por_instancia.csv con {len(df_instancia)} registros.")

# --- TABLA FORMATO PAPER ---
df_paper = df.groupby(["Clase", "Algoritmo"]).agg({
    "Avg_Vehiculos": "mean",
    "Avg_Distancia": "mean"
}).reset_index()

df_paper_pivot = df_paper.pivot(index="Clase", columns="Algoritmo", values=["Avg_Vehiculos", "Avg_Distancia"])
df_paper_pivot.columns = [f"{col[0]}_{col[1]}" for col in df_paper_pivot.columns]
df_paper_pivot.reset_index(inplace=True)

bks_df = df.groupby("Clase").agg({"BKS_Vehiculos": "mean", "BKS_Distancia": "mean"}).reset_index()
df_paper_final = pd.merge(bks_df, df_paper_pivot, on="Clase")

cat_type = pd.CategoricalDtype(categories=["C1", "C2", "R1", "R2", "RC1", "RC2"], ordered=True)
df_paper_final["Clase"] = df_paper_final["Clase"].astype(cat_type)
df_paper_final = df_paper_final.sort_values("Clase")

def fmt_nv_td(nv, td):
    return f"{nv:.2f} / {td:.2f}"

paper_records = []
for _, row in df_paper_final.iterrows():
    bks_str = fmt_nv_td(row["BKS_Vehiculos"], row["BKS_Distancia"])
    
    classic_nv = row.get("Avg_Vehiculos_CLASSIC", row["BKS_Vehiculos"])
    classic_td = row.get("Avg_Distancia_CLASSIC", row["BKS_Distancia"])
    ql_nv = row.get("Avg_Vehiculos_QLEARNING", row["BKS_Vehiculos"])
    ql_td = row.get("Avg_Distancia_QLEARNING", row["BKS_Distancia"])
    
    classic_str = fmt_nv_td(classic_nv, classic_td)
    ql_str = fmt_nv_td(ql_nv, ql_td)
    
    gap_bks_nv = ((ql_nv - row["BKS_Vehiculos"]) / row["BKS_Vehiculos"]) * 100 if row["BKS_Vehiculos"] > 0 else 0
    gap_bks_td = ((ql_td - row["BKS_Distancia"]) / row["BKS_Distancia"]) * 100 if row["BKS_Distancia"] > 0 else 0
    
    imp_alns_nv = ((classic_nv - ql_nv) / classic_nv) * 100 if classic_nv > 0 else 0
    imp_alns_td = ((classic_td - ql_td) / classic_td) * 100 if classic_td > 0 else 0
    
    gap_str = f"{gap_bks_nv:+.2f}% / {gap_bks_td:+.2f}%"
    imp_str = f"{imp_alns_nv:+.2f}% / {imp_alns_td:+.2f}%"

    cost_bks_row = row["BKS_Vehiculos"] * VEHICLE_WEIGHT + row["BKS_Distancia"]
    cost_classic_row = classic_nv * VEHICLE_WEIGHT + classic_td
    cost_ql_row = ql_nv * VEHICLE_WEIGHT + ql_td

    gap_uni_classic = ((cost_classic_row - cost_bks_row) / cost_bks_row) * 100 if cost_bks_row > 0 else 0
    gap_uni_ql = ((cost_ql_row - cost_bks_row) / cost_bks_row) * 100 if cost_bks_row > 0 else 0
    imp_uni = ((cost_classic_row - cost_ql_row) / cost_classic_row) * 100 if cost_classic_row > 0 else 0

    paper_records.append({
        "Benchmark / Clase": row["Clase"],
        "BKS (NV/TD)": bks_str,
        "Classical ALNS (NV/TD)": classic_str,
        "Proposed RL-ALNS (NV/TD)": ql_str,
        "Gap vs. BKS (NV/TD %)": gap_str,
        "Imp. vs. ALNS (NV/TD %)": imp_str,
        "Gap Unificado ALNS vs. BKS (%)": f"{gap_uni_classic:+.2f}%",
        "Gap Unificado Q-ALNS vs. BKS (%)": f"{gap_uni_ql:+.2f}%",
        "Imp. Unificado vs. ALNS (%)": f"{imp_uni:+.2f}%"
    })

avg_bks_nv = df_paper_final["BKS_Vehiculos"].mean()
avg_bks_td = df_paper_final["BKS_Distancia"].mean()
avg_classic_nv = df_paper_final.get("Avg_Vehiculos_CLASSIC", df_paper_final["BKS_Vehiculos"]).mean()
avg_classic_td = df_paper_final.get("Avg_Distancia_CLASSIC", df_paper_final["BKS_Distancia"]).mean()
avg_ql_nv = df_paper_final.get("Avg_Vehiculos_QLEARNING", df_paper_final["BKS_Vehiculos"]).mean()
avg_ql_td = df_paper_final.get("Avg_Distancia_QLEARNING", df_paper_final["BKS_Distancia"]).mean()

gap_bks_nv_global = ((avg_ql_nv - avg_bks_nv) / avg_bks_nv) * 100 if avg_bks_nv > 0 else 0
gap_bks_td_global = ((avg_ql_td - avg_bks_td) / avg_bks_td) * 100 if avg_bks_td > 0 else 0
imp_alns_nv_global = ((avg_classic_nv - avg_ql_nv) / avg_classic_nv) * 100 if avg_classic_nv > 0 else 0
imp_alns_td_global = ((avg_classic_td - avg_ql_td) / avg_classic_td) * 100 if avg_classic_td > 0 else 0

cost_bks_global = avg_bks_nv * VEHICLE_WEIGHT + avg_bks_td
cost_classic_global = avg_classic_nv * VEHICLE_WEIGHT + avg_classic_td
cost_ql_global = avg_ql_nv * VEHICLE_WEIGHT + avg_ql_td

gap_uni_classic_global = ((cost_classic_global - cost_bks_global) / cost_bks_global) * 100 if cost_bks_global > 0 else 0
gap_uni_ql_global = ((cost_ql_global - cost_bks_global) / cost_bks_global) * 100 if cost_bks_global > 0 else 0
imp_uni_global = ((cost_classic_global - cost_ql_global) / cost_classic_global) * 100 if cost_classic_global > 0 else 0

paper_records.append({
    "Benchmark / Clase": "Promedio Global",
    "BKS (NV/TD)": fmt_nv_td(avg_bks_nv, avg_bks_td),
    "Classical ALNS (NV/TD)": fmt_nv_td(avg_classic_nv, avg_classic_td),
    "Proposed RL-ALNS (NV/TD)": fmt_nv_td(avg_ql_nv, avg_ql_td),
    "Gap vs. BKS (NV/TD %)": f"{gap_bks_nv_global:+.2f}% / {gap_bks_td_global:+.2f}%",
    "Imp. vs. ALNS (NV/TD %)": f"{imp_alns_nv_global:+.2f}% / {imp_alns_td_global:+.2f}%",
    "Gap Unificado ALNS vs. BKS (%)": f"{gap_uni_classic_global:+.2f}%",
    "Gap Unificado Q-ALNS vs. BKS (%)": f"{gap_uni_ql_global:+.2f}%",
    "Imp. Unificado vs. ALNS (%)": f"{imp_uni_global:+.2f}%"
})

df_paper_out = pd.DataFrame(paper_records)
df_paper_out.to_csv(os.path.join(TABLAS_DIR, "tabla_paper_format.csv"), index=False)
print(f"-> Generada tabla: tabla_paper_format.csv (Formato RL-ALNS).")

print("\n--- TABLA PRINCIPAL DE RESULTADOS (FORMATO PAPER) ---")
headers = df_paper_out.columns.tolist()
col_widths = {h: max(len(h), df_paper_out[h].astype(str).map(len).max()) for h in headers}
header_format = " | ".join(f"{{:<{col_widths[h]}}}" for h in headers)
sep_len = sum(col_widths.values()) + 3 * (len(headers) - 1)

print(header_format.format(*headers))
print("-" * sep_len)
for _, row in df_paper_out.iterrows():
    print(header_format.format(*[str(row[h]) for h in headers]))
print("-" * sep_len)

print("\n=== FIN DEL ANÁLISIS ===")