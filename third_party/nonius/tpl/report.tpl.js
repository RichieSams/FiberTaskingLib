!function () {
    var data = {
        title: '{$title}',
        units: '{$units}',
        logarithmic: {$logarithmic},
        param: '{$runparam}',
        runs: [
            {% for run in runs %}{
                params: {
                    {% for param in run.params %}'{$param.name}': '{$param.value}',
                    {% endfor %}
                },
                benchmarks: [
                    {% for benchmark in run.benchmarks %}{
                        name: '{$benchmark.name}',
                        mean: {$benchmark.mean},
                        stddev: {$benchmark.stddev},
                        samples: [
                            {% for sample in benchmark.samples %}{$sample}, {% endfor %}
                        ],
                    },{% endfor %}
                ]
            },{% endfor %}
        ]
    };

    var plotdiv = document.getElementById("plot");
    window.addEventListener("resize", function() {
        Plotly.Plots.resize(plotdiv);
    });

    var chooser = document.getElementById("chooser");
    chooser.addEventListener("change", choosePlot);
    chooser.addEventListener("blur", chooser.focus.bind(chooser));
    chooser.focus();

    var legendStyle = {
        font: { family: 'monospace' },
        borderwidth: 2,
        bordercolor: 'black'
    }

    function choosePlot() {
        var plot = chooser.options[chooser.selectedIndex].value;
        if (plot == 'summary') {
            if (data.runs.length > 1) {
                plotSummary();
            } else {
                plotSingleSummary();
            }
        } else {
            plotSamples(plot);
        }
    }

    function plotSamples(plot) {
        var run = data.runs[plot];
        var traces = run.benchmarks.map(function (b, i) {
            return {
                name: b.name,
                type: 'scatter',
                mode: 'markers',
                marker: { symbol: i },
                y: b.samples,
                x: b.samples.map(function (_, i) { return i; })
            }
        });
        var layout = {
            title: data.title,
            showLegend: true,
            xaxis: { title: 'Measurement' },
            yaxis: {
                title: 'Time (' + data.units + ')',
                rangemode: 'tozero',
                zeroline: true
            },
            legend: legendStyle
        };
        Plotly.newPlot(plotdiv, traces, layout);
    }

    function plotSummary() {
        var traces = data.runs[0].benchmarks.map(function (b, i) {
            return {
                name: b.name,
                type: 'scatter',
                marker: { symbol: i },
                x: data.runs.map(function (r) { return r.params[data.param]; }),
                y: data.runs.map(function (r) { return r.benchmarks[i].mean; }),
                error_y: {
                    type: 'data',
                    array: data.runs.map(function (r) { return r.benchmarks[i].stddev; }),
                    visible: true
                }
            }
        });
        var layout = {
            title: data.title,
            showLegend: true,
            xaxis: {
                title: data.param,
                type: data.logarithmic ? 'log' : '',
            },
            yaxis: {
                title: 'Time (' + data.units + ')',
                rangemode: 'tozero',
                zeroline: true,
                type: data.logarithmic ? 'log' : '',
            },
            legend: legendStyle
        };
        Plotly.newPlot(plotdiv, traces, layout);
    }

    function plotSingleSummary() {
        var traces = data.runs[0].benchmarks.map(function (b, i) {
            return {
                type: 'bar',
                name: b.name,
                x: [ 0 ],
                y: [ b.mean ],
                error_y: {
                    type: 'data',
                    array: [ b.stddev ],
                    visible: true
                }
            }
        });
        var layout = {
            title: data.title,
            showLegend: true,
            xaxis: {
                title: '',
                showticklabels: false,
            },
            yaxis: {
                title: 'Time (' + data.units + ')',
                rangemode: 'tozero',
                zeroline: true
            },
            legend: legendStyle
        };
        Plotly.newPlot(plotdiv, traces, layout);
    }

    choosePlot();
}();
