#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
import time
import argparse


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("filename", nargs=1)
    parser.add_argument("--rows", type=int)
    parser.add_argument("--smooth", type=int, default=10)
    parser.add_argument("--refresh-every", type=float, default=20.0)
    args = parser.parse_args()

    data = pd.read_csv(args.filename[0])
    plots = len(data.columns) - 1
    cols = 2
    rows = (plots + (cols - 1)) // cols

    fig, axs = plt.subplots(rows, cols)

    running = True

    def on_close(event):
        nonlocal running
        running = False

    fig.canvas.mpl_connect("close_event", on_close)

    while running:
        data = pd.read_csv(args.filename[0])
        if args.rows:
            data = data.tail(args.rows)

        first_col = data.columns[0]

        def create_plots():
            for i in range(rows):
                for j in range(cols):
                    yield axs[i, j]

        actual_smooth = min(args.smooth, len(data) // 2)
        plots = create_plots()
        for i, col in enumerate(data.columns[1:]):
            ax = next(plots)
            ax.clear()
            col_data = data[col]
            if actual_smooth > 1:
                col_data = col_data.rolling(actual_smooth).mean()
            ax.plot(col_data, linewidth=1.0)
            ax.set_title(col)

        plt.pause(args.refresh_every)


if __name__ == "__main__":
    main()
