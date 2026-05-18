// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Chart, ChartDataLabels } from '../../../vendor/chartjs/chart.bundle.js';
import RESULTS from './benchmarks.json' with { type: 'json' };

const ENGINES = ['Node-API', 'Koffi v3', 'Koffi v2', 'node-ctypes', 'ffi-rs', 'ffi-napi'];
const COLORS = ['#E63946', '#F4A261', '#E9C46A', '#2A9D8F', '#457B9D', '#6A4C93', '#FF006E'];

initCharts();
initTables();

function initCharts() {
    let divs = document.querySelectorAll('div.benchmark.chart');

    for (let div of divs) {
        let platform = div.dataset.platform;
        let data = RESULTS[platform];

        if (data == null)
            continue;

        let benchmarks = Object.keys(data.results);

        let datasets = ENGINES.map((engine, idx) => ({
            label: engine,
            data: benchmarks.map(benchmark => data.results[benchmark][engine]?.ratio),
            backgroundColor: COLORS[idx],
            borderColor: COLORS[idx]
        }));

        let canvas = document.createElement('canvas');
        let ctx = canvas.getContext('2d');

        new Chart(ctx, {
            plugins: [ChartDataLabels],
            type: 'bar',
            data: {
                datasets: datasets,
                labels: benchmarks
            },
            options: {
                maintainAspectRatio: false,
                indexAxis: 'y',
                responsive: true,
                scales: {
                    x: {
                        max: 1.2,
                        ticks: {
                            font: {
                                family: 'Open Sans',
                                size: 12
                            }
                        }
                    },
                    y: {
                        ticks: {
                            font: {
                                family: 'Open Sans',
                                size: 14,
                                weight: 'bold'
                            }
                        }
                    }
                },
                plugins: {
                    title: {
                        display: true,
                        text: data.title,
                        font: {
                            family: 'Open Sans',
                            size: 16,
                            weight: 'bold'
                        }
                    },
                    legend: false,
                    datalabels: {
                        align: 'end',
                        anchor: 'end',
                        font: {
                            family: 'Open Sans',
                            size: 14
                        },
                        formatter: (value, ctx) => {
                            let prefix = ctx.dataset.label;

                            if (value == null) {
                                return prefix + ' (not tested)';
                            } else if (ctx.dataset.label == ENGINES[0]) {
                                return prefix + ' (ref)';
                            } else {
                                return prefix + ' x' + value?.toFixed?.(2);
                            }
                        }
                    },
                    tooltip: false
                }
            }
        });

        render(canvas, div);
    }
}

function initTables() {
    let divs = document.querySelectorAll('div.benchmark.table');

    for (let div of divs) {
        let platform = div.dataset.platform;
        let benchmark = div.dataset.benchmark;
        let results = RESULTS[platform]?.results?.[benchmark];

        if (results == null)
            continue;

        render(html`
            <table>
                <thead>
                    <tr>
                        <th>${benchmark}</th>
                        <th>Iteration time</th>
                        <th>Ratio</th>
                        <th>Overhead</th>
                    </tr>
                </thead>
                <tbody>
                    ${Object.keys(results).map(engine => {
                        let perf = results[engine];

                        return html`
                            <tr>
                                <td>${engine}</td>
                                <td>${formatTime(perf.time / perf.iterations)}</td>
                                <td>x${perf.ratio.toFixed(2)}</td>
                                <td>${Math.round(perf.overhead * 100)}%</td>
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        `, div);
    }
}

function formatTime(time, unit = 'ns') {
    switch (unit) {
        case 'ms': { return (time * 1).toFixed(1) + ' ms'; } break;
        case 'us': { return (time * 1000).toFixed(1) + ' µs'; } break;
        case 'ns': { return (time * 10000000).toFixed(0) + ' ns'; } break;
    }
}
