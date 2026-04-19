"""
IFT729 – L02 : Génération des graphiques de performance
Étudiant : Carlos Tsambou Jiofack (TSAC1701)
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os

OUT = "/home/claude/results"
MODES = ["NORMAL", "STRESS", "FORCED_OVERRUN", "SENSOR_FAILURE"]
COLORS = {"NORMAL": "#2E75B6", "STRESS": "#ED7D31", "FORCED_OVERRUN": "#C00000", "SENSOR_FAILURE": "#7030A0"}
LABELS = {"NORMAL": "Normal (baseline)", "STRESS": "Stress CPU", "FORCED_OVERRUN": "Dépassement forcé", "SENSOR_FAILURE": "Panne capteurs 30%"}

dfs = {}
for m in MODES:
    path = f"{OUT}/perf_{m}.csv"
    if os.path.exists(path):
        dfs[m] = pd.read_csv(path)

# ============================================================
# Figure 1 : Temps d'analyse par mode (barres comparatives)
# ============================================================
fig, axes = plt.subplots(1, 2, figsize=(14, 5))
fig.suptitle("IFT729 – L02 : Performance des modules TR\nCarlos Tsambou Jiofack (TSAC1701)", fontsize=13, fontweight='bold')

# Analyse duration
ax = axes[0]
means, maxs, labels = [], [], []
for m in MODES:
    if m in dfs:
        v = dfs[m]['analyse_us'].dropna() / 1000.0
        means.append(v.mean())
        maxs.append(v.max())
        labels.append(LABELS[m])

x = np.arange(len(labels))
width = 0.35
bars1 = ax.bar(x - width/2, means, width, label='Moyenne', color=[COLORS[m] for m in MODES if m in dfs], alpha=0.85)
bars2 = ax.bar(x + width/2, maxs,  width, label='Maximum', color=[COLORS[m] for m in MODES if m in dfs], alpha=0.45, edgecolor='black', linewidth=0.8)

ax.axhline(y=20, color='red', linestyle='--', linewidth=1.5, label='Deadline (20 ms)')
ax.set_title("Module Analyse — Temps d'exécution (ms)", fontsize=11)
ax.set_ylabel("Temps (ms)")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=12, fontsize=9)
ax.legend(fontsize=9)
ax.set_ylim(0, max(maxs) * 1.25 if maxs else 35)
ax.grid(axis='y', alpha=0.3)

for bar in bars1:
    h = bar.get_height()
    ax.annotate(f'{h:.1f}', xy=(bar.get_x() + bar.get_width()/2, h), xytext=(0, 3),
                textcoords="offset points", ha='center', fontsize=8)

# Alerte latency
ax = axes[1]
means2, maxs2 = [], []
for m in MODES:
    if m in dfs:
        v = dfs[m]['alerte_latence_us'].dropna()
        v = v[v > 0] / 1000.0
        means2.append(v.mean() if len(v) > 0 else 0)
        maxs2.append(v.max() if len(v) > 0 else 0)

bars3 = ax.bar(x - width/2, means2, width, label='Moyenne', color=[COLORS[m] for m in MODES if m in dfs], alpha=0.85)
bars4 = ax.bar(x + width/2, maxs2,  width, label='Maximum', color=[COLORS[m] for m in MODES if m in dfs], alpha=0.45, edgecolor='black', linewidth=0.8)

ax.axhline(y=100, color='red', linestyle='--', linewidth=1.5, label='Deadline (100 ms)')
ax.set_title("Module Alerte — Latence totale (ms)", fontsize=11)
ax.set_ylabel("Latence (ms)")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=12, fontsize=9)
ax.legend(fontsize=9)
ax.set_ylim(0, max(maxs2) * 1.4 if maxs2 else 110)
ax.grid(axis='y', alpha=0.3)

for bar in bars3:
    h = bar.get_height()
    ax.annotate(f'{h:.1f}', xy=(bar.get_x() + bar.get_width()/2, h), xytext=(0, 3),
                textcoords="offset points", ha='center', fontsize=8)

plt.tight_layout()
plt.savefig(f"{OUT}/fig1_performance_comparee.png", dpi=150, bbox_inches='tight')
plt.close()
print("Figure 1 sauvegardée.")

# ============================================================
# Figure 2 : Timeline Analyse — mode NORMAL vs FORCED_OVERRUN
# ============================================================
fig, axes = plt.subplots(2, 1, figsize=(14, 7))
fig.suptitle("IFT729 – L02 : Stabilité temporelle du module Analyse\nCarlos Tsambou Jiofack (TSAC1701)", fontsize=13, fontweight='bold')

for i, (m, ax) in enumerate(zip(["NORMAL", "FORCED_OVERRUN"], axes)):
    if m not in dfs:
        continue
    df = dfs[m]
    v = df['analyse_us'] / 1000.0
    cycles = df['cycle']
    ax.plot(cycles, v, color=COLORS[m], linewidth=0.8, alpha=0.8)
    ax.axhline(y=20, color='red', linestyle='--', linewidth=1.5, label='Deadline 20 ms')
    ax.fill_between(cycles, v, 20, where=(v > 20), color='red', alpha=0.3, label='Dépassement')
    ax.set_title(f"Mode {LABELS[m]}", fontsize=10)
    ax.set_ylabel("Temps analyse (ms)")
    ax.set_xlabel("Numéro de cycle")
    ax.legend(fontsize=9)
    ax.grid(alpha=0.3)
    ax.set_ylim(0, max(v.max() * 1.2, 25))

plt.tight_layout()
plt.savefig(f"{OUT}/fig2_timeline_analyse.png", dpi=150, bbox_inches='tight')
plt.close()
print("Figure 2 sauvegardée.")

# ============================================================
# Figure 3 : Distribution des types d'alertes par mode
# ============================================================
fig, axes = plt.subplots(2, 2, figsize=(14, 9))
fig.suptitle("IFT729 – L02 : Distribution des types d'alertes par scénario\nCarlos Tsambou Jiofack (TSAC1701)", fontsize=13, fontweight='bold')
axes = axes.flatten()

alert_keywords = {
    "Tachycardie":   "TACHYCARDIE",
    "Bradycardie":   "BRADYCARDIE",
    "Hypoxie":       "HYPOXIE",
    "Tachypnée":     "TACHYPNEE",
    "Panne ECG":     "ECG défaillant",
    "Panne SpO2":    "SpO2 défaillant",
    "Panne Resp":    "Resp défaillant",
}
palette = ["#2E75B6", "#ED7D31", "#A9D18E", "#FFD966", "#C00000", "#7030A0", "#00B0F0"]

for i, m in enumerate(MODES):
    ax = axes[i]
    if m not in dfs:
        ax.set_visible(False)
        continue
    df = dfs[m][dfs[m]['has_alerte'] == 1]
    counts = {}
    for label, kw in alert_keywords.items():
        counts[label] = df['type'].str.contains(kw, na=False).sum()
    counts = {k: v for k, v in counts.items() if v > 0}
    if counts:
        ax.pie(counts.values(), labels=counts.keys(), autopct='%1.0f%%',
               colors=palette[:len(counts)], startangle=90,
               textprops={'fontsize': 9})
    ax.set_title(f"{LABELS[m]}\n({len(df)} alertes)", fontsize=10)

plt.tight_layout()
plt.savefig(f"{OUT}/fig3_distribution_alertes.png", dpi=150, bbox_inches='tight')
plt.close()
print("Figure 3 sauvegardée.")

# ============================================================
# Figure 4 : Taux de dépassement et résumé
# ============================================================
fig, ax = plt.subplots(figsize=(10, 5))
fig.suptitle("IFT729 – L02 : Taux de dépassement de deadline par scénario\nCarlos Tsambou Jiofack (TSAC1701)", fontsize=13, fontweight='bold')

labels_plot, miss_analyse, miss_alerte, total_cycles = [], [], [], []
for m in MODES:
    if m not in dfs:
        continue
    df = dfs[m]
    n = len(df)
    ma = (df['analyse_ok'] == 0).sum()
    mal = (df['alerte_ok'] == 0).sum() if 'alerte_ok' in df.columns else 0
    labels_plot.append(LABELS[m])
    miss_analyse.append(ma / n * 100)
    miss_alerte.append(mal / n * 100)
    total_cycles.append(n)

x = np.arange(len(labels_plot))
width = 0.35
ax.bar(x - width/2, miss_analyse, width, label='Analyse (deadline 20ms)', color='#ED7D31', alpha=0.85)
ax.bar(x + width/2, miss_alerte,  width, label='Alerte (deadline 100ms)', color='#2E75B6', alpha=0.85)

for j, (ma, mal, nc) in enumerate(zip(miss_analyse, miss_alerte, total_cycles)):
    ax.annotate(f'{ma:.0f}%', xy=(x[j]-width/2, ma), xytext=(0, 3), textcoords="offset points", ha='center', fontsize=9)
    ax.annotate(f'{mal:.0f}%', xy=(x[j]+width/2, mal), xytext=(0, 3), textcoords="offset points", ha='center', fontsize=9)

ax.set_ylabel("Taux de dépassement (%)")
ax.set_xticks(x)
ax.set_xticklabels(labels_plot, rotation=10, fontsize=10)
ax.legend(fontsize=10)
ax.grid(axis='y', alpha=0.3)
ax.set_ylim(0, max(max(miss_analyse), max(miss_alerte)) * 1.3 + 5)

plt.tight_layout()
plt.savefig(f"{OUT}/fig4_taux_depassement.png", dpi=150, bbox_inches='tight')
plt.close()
print("Figure 4 sauvegardée.")
print("\nTous les graphiques générés dans", OUT)
