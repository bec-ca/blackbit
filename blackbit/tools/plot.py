#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import numpy.typing as npt
import pandas as pd
import time
import argparse
import typing
from scipy import optimize


class Polynomial:
    def __init__(self, weights: npt.NDArray):
        self.weights = weights

    @staticmethod
    def with_degree(degree: int):
        return Polynomial(np.array([1] * (degree + 1)))

    def eval(self, x, *coefs):
        out = 0.0
        for c, w in zip(coefs, self.weights):
            out += out * x + c * w
        return out

    def sample_input(self, value=1.0):
        return np.array([value] * len(self.weights))

    def gradient(self):
        new_weights = []
        degree = self.num_variables()
        for i, w in enumerate(self.weights[:-1]):
            new_weights.append(w * (degree - i))
        return Polynomial(np.array(new_weights))

    def num_variables(self):
        return len(self.weights) - 1


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("--sort-by")
    parser.add_argument("--metric")
    parser.add_argument("--avg", type=float)
    parser.add_argument("--degree", type=int, default=8)
    parser.add_argument("filename", nargs=1)
    args = parser.parse_args()

    degree : int = args.degree
    objective = Polynomial.with_degree(degree)

    objective_grad1 = objective.gradient()
    objective_grad2 = objective_grad1.gradient()

    data : typing.Any = pd.read_csv(args.filename[0])

    # if 'changed' in data.columns:
    #     data = data[data['changed'] == True]

    if args.sort_by is not None:
        x_axis = args.sort_by
        data = data.sort_values([x_axis])
    else:
        x_axis = "test_score"

    data_points = len(data)
    data.index = np.arange(data_points)

    metric = args.metric or "test_score"

    if not (metric in data.columns):
        raise RuntimeError(
            "metric {} not available, available metrics: {}".format(
                metric, data.columns
            )
        )

    avg_metric = args.avg or data[metric].mean()
    print("Avg_metric: {}".format(avg_metric))

    data["expected"] = avg_metric

    data["game_num"] = 1
    data["game_fraction"] = data["game_num"].cumsum() / data_points
    data["percentile"] = data["game_fraction"] * 100.0

    data["test_sum"] = data[metric].cumsum() / data_points

    data["expected_sum"] = data["expected"].cumsum() / data_points

    data["test_sum_diff"] = data["test_sum"] - data["expected_sum"]

    ts = time.time()
    params, _ = optimize.curve_fit(
        objective.eval,
        data["game_fraction"],
        data["test_sum_diff"],
        objective.sample_input(0.1),
    )
    print(f"Fitting took {time.time() - ts:.3f}s")

    data["fitted"] = objective.eval(data["game_fraction"], *params)
    data["fitted_grad1"] = objective_grad1.eval(data["game_fraction"], *params)
    data["fitted_grad2"] = objective_grad2.eval(data["game_fraction"], *params)

    data_mult = data * 100
    print(data_mult["game_fraction"])
    data_mult.plot("game_fraction", ["test_sum_diff", "fitted"])
    data_mult.plot("game_fraction", ["fitted_grad1"])
    # data_mult.plot("game_fraction", ["fitted_grad2"])

    idx = data["fitted_grad1"].idxmax()
    max_metric = data[x_axis][idx]
    print("max_slop {}:{}".format(x_axis, max_metric))

    roots = np.where(np.diff(np.sign(data["fitted_grad1"])))[0]
    print("grad1 roots: ", roots)
    print(
        "grad1 root metrics: ",
        [
            "{:.3f}% {}".format(data["game_fraction"][idx] * 100.0, data[x_axis][idx])
            for idx in roots
        ],
    )

    roots = np.where(np.diff(np.sign(data["fitted_grad2"])))[0]
    print("grad2 roots: ", roots)
    print(
        "grad2 root metrics: ",
        [
            "{:.3f}% {} {:.3f}%".format(
                data["game_fraction"][idx] * 100.0,
                data[x_axis][idx],
                data["fitted_grad1"][idx] * 100.0,
            )
            for idx in roots
        ],
    )

    plt.show()


if __name__ == "__main__":
    main()
