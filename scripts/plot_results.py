#!/usr/bin/env python3
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def save_pivot_table(df: pd.DataFrame, out_csv: Path, out_md: Path) -> None:
    df.to_csv(out_csv, index=False)
    # Markdown table for easy copy/paste into report
    with out_md.open("w", encoding="utf-8") as f:
        f.write(df.to_markdown(index=False))


def plot_lines(df: pd.DataFrame, x_col: str, y_col: str, group_col: str, title: str, out_png: Path) -> None:
    plt.figure()
    for key, g in df.groupby(group_col):
        g = g.sort_values(x_col)
        plt.plot(g[x_col], g[y_col], marker="o", label=str(key))
    plt.xlabel(x_col)
    plt.ylabel(y_col)
    plt.title(title)
    plt.grid(True, linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_png, dpi=200)
    plt.close()


def main():
    if len(sys.argv) < 3:
        print("Usage: plot_results.py <results_csv> <out_dir> [queue_fixed] [threads_fixed]")
        sys.exit(1)

    results_csv = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    queue_fixed = int(sys.argv[3]) if len(sys.argv) >= 4 else 50
    threads_fixed = int(sys.argv[4]) if len(sys.argv) >= 5 else 8

    ensure_dir(out_dir)
    tables_dir = out_dir / "tables"
    figs_dir = out_dir / "figures"
    ensure_dir(tables_dir)
    ensure_dir(figs_dir)

    df = pd.read_csv(results_csv)

    # Basic sanity check
    required = {"policy", "threads", "queue_size", "throughput", "avg_latency", "min_latency", "max_latency", "wall_time"}
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"Missing columns in CSV: {sorted(missing)}")

    # If latency is in seconds, convert to ms for plotting (optional but nicer)
    # Comment these 4 lines out if you prefer seconds.
    for col in ["avg_latency", "min_latency", "max_latency", "wall_time"]:
        df[col] = df[col] * 1000.0

    # ----------------------------
    # Figure 1 & 2: vs threads (queue fixed)
    # ----------------------------
    df_q = df[df["queue_size"] == queue_fixed].copy()
    if df_q.empty:
        print(f"Warning: no rows for queue_size={queue_fixed}. Available: {sorted(df['queue_size'].unique())}")

    # Aggregate if you have repeats: average per (policy,threads,queue_size)
    df_q_agg = (
        df_q.groupby(["policy", "threads"], as_index=False)
           .agg(throughput=("throughput", "mean"),
                avg_latency=("avg_latency", "mean"),
                min_latency=("min_latency", "mean"),
                max_latency=("max_latency", "mean"),
                wall_time=("wall_time", "mean"))
    )

    plot_lines(
        df_q_agg, "threads", "throughput", "policy",
        f"Throughput vs Threads (queue_size={queue_fixed})",
        figs_dir / f"throughput_vs_threads_q{queue_fixed}.png"
    )
    plot_lines(
        df_q_agg, "threads", "avg_latency", "policy",
        f"Avg Latency vs Threads (queue_size={queue_fixed}) [ms]",
        figs_dir / f"avg_latency_vs_threads_q{queue_fixed}.png"
    )

    # Tables (nice for report)
    save_pivot_table(
        df_q_agg.sort_values(["policy", "threads"]),
        tables_dir / f"summary_threads_q{queue_fixed}.csv",
        tables_dir / f"summary_threads_q{queue_fixed}.md"
    )

    # ----------------------------
    # Figure 3 & 4: vs queue_size (threads fixed)
    # ----------------------------
    df_t = df[df["threads"] == threads_fixed].copy()
    if df_t.empty:
        print(f"Warning: no rows for threads={threads_fixed}. Available: {sorted(df['threads'].unique())}")

    df_t_agg = (
        df_t.groupby(["policy", "queue_size"], as_index=False)
           .agg(throughput=("throughput", "mean"),
                avg_latency=("avg_latency", "mean"),
                min_latency=("min_latency", "mean"),
                max_latency=("max_latency", "mean"),
                wall_time=("wall_time", "mean"))
    )

    plot_lines(
        df_t_agg, "queue_size", "throughput", "policy",
        f"Throughput vs Queue Size (threads={threads_fixed})",
        figs_dir / f"throughput_vs_queue_t{threads_fixed}.png"
    )
    plot_lines(
        df_t_agg, "queue_size", "avg_latency", "policy",
        f"Avg Latency vs Queue Size (threads={threads_fixed}) [ms]",
        figs_dir / f"avg_latency_vs_queue_t{threads_fixed}.png"
    )

    save_pivot_table(
        df_t_agg.sort_values(["policy", "queue_size"]),
        tables_dir / f"summary_queue_t{threads_fixed}.csv",
        tables_dir / f"summary_queue_t{threads_fixed}.md"
    )

    print("Done.")
    print(f"Figures: {figs_dir}")
    print(f"Tables:  {tables_dir}")


if __name__ == "__main__":
    main()
