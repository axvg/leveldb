#!/usr/bin/env python3
"""Turn the raw benchmark CSVs into thesis data files and LaTeX tables.

Reads:
  results/bench.csv        (config,epsilon,benchmark,rep,micros_per_op,mb_per_s)
  results/model_stats.csv  (dist,blocks,epsilon,segments,model_bytes)

Writes into the thesis tree (../../../../tesis_latex):
  data/read_latency.dat    epsilon vs learned micros/op (per benchmark)
  data/model_size.dat      epsilon vs segments (skewed distribution)
  tables/read_latency.tex  booktabs table: baseline vs learned + speedup
  tables/model_size.tex    booktabs table: model size vs epsilon

Uses only pandas + numpy (already in the venv). No matplotlib: the charts are
drawn with pgfplots inside the Docker LaTeX build from the .dat files.
"""
import os
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "results")
THESIS = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "tesis_latex"))
DATA = os.path.join(THESIS, "data")
TABLES = os.path.join(THESIS, "tables")
os.makedirs(DATA, exist_ok=True)
os.makedirs(TABLES, exist_ok=True)

READ_BENCHES = ["readrandom", "readmissing", "readseq"]


def load_bench():
    df = pd.read_csv(os.path.join(RESULTS, "bench.csv"))
    # Median micros/op per (config, epsilon, benchmark).
    med = (df.groupby(["config", "epsilon", "benchmark"])["micros_per_op"]
             .median().reset_index())
    return med


def write_read_latency(med):
    base = med[med.config == "baseline"].set_index("benchmark")["micros_per_op"]
    learned = med[med.config == "learned"]

    # .dat for pgfplots: one column per benchmark, rows = epsilon.
    epsilons = sorted(learned.epsilon.unique())
    lines = ["epsilon " + " ".join(READ_BENCHES) + " baseline_readrandom"]
    for eps in epsilons:
        row = [str(eps)]
        for b in READ_BENCHES:
            v = learned[(learned.epsilon == eps) & (learned.benchmark == b)]
            row.append(f"{v.micros_per_op.iloc[0]:.4f}" if len(v) else "nan")
        row.append(f"{base.get('readrandom', float('nan')):.4f}")
        lines.append(" ".join(row))
    with open(os.path.join(DATA, "read_latency.dat"), "w") as f:
        f.write("\n".join(lines) + "\n")

    # LaTeX table: for each benchmark, baseline vs best learned + speedup.
    best = (learned.loc[learned.groupby("benchmark")["micros_per_op"].idxmin()]
                   .set_index("benchmark"))
    rows = []
    for b in READ_BENCHES:
        bl = base.get(b, float("nan"))
        le = best.loc[b, "micros_per_op"]
        eps = int(best.loc[b, "epsilon"])
        sp = (bl - le) / bl * 100.0
        rows.append(f"{b} & {bl:.3f} & {le:.3f} & {eps} & {sp:+.1f}\\% \\\\")
    table = r"""\begin{tabular}{lrrrr}
\toprule
Operación & LevelDB ($\mu$s/op) & Learned ($\mu$s/op) & $\varepsilon^{*}$ & Mejora \\
\midrule
""" + "\n".join(rows) + r"""
\bottomrule
\end{tabular}"""
    with open(os.path.join(TABLES, "read_latency.tex"), "w") as f:
        f.write(table + "\n")
    return best


def write_model_size():
    df = pd.read_csv(os.path.join(RESULTS, "model_stats.csv"))
    sk = df[(df.dist == "skewed") & (df.blocks == 2000)].sort_values("epsilon")
    with open(os.path.join(DATA, "model_size.dat"), "w") as f:
        f.write("epsilon segments bytes\n")
        for _, r in sk.iterrows():
            f.write(f"{int(r.epsilon)} {int(r.segments)} {int(r.model_bytes)}\n")

    rows = [f"{int(r.epsilon)} & {int(r.segments)} & {int(r.model_bytes)} \\\\"
            for _, r in sk.iterrows()]
    table = r"""\begin{tabular}{rrr}
\toprule
$\varepsilon$ & Segmentos & Tamaño del modelo (bytes) \\
\midrule
""" + "\n".join(rows) + r"""
\bottomrule
\end{tabular}"""
    with open(os.path.join(TABLES, "model_size.tex"), "w") as f:
        f.write(table + "\n")


def main():
    med = load_bench()
    best = write_read_latency(med)
    write_model_size()
    print("Wrote thesis data/tables. Best learned read latencies:")
    print(best[["epsilon", "micros_per_op"]])


if __name__ == "__main__":
    main()
