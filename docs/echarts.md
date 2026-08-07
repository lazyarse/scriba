# ECharts Chart Examples

This file demonstrates the chart types supported by ECharts. Scriba renders ` ```ec `
blocks natively in the preview pane. Each chart is an ECharts option object (see the
[ECharts docs](https://echarts.apache.org/) for the full option reference). Open
this file alongside the preview to see each chart rendered.

---

## Line Chart

Trends and continuous data over time.

```ec
{
  "tooltip": {"trigger": "axis"},
  "legend": {"data": ["Sales", "Target"]},
  "grid": {"left": "3%", "right": "4%", "top": "8%", "bottom": "3%", "containLabel": true},
  "xAxis": {
    "type": "category",
    "data": ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
  },
  "yAxis": {"type": "value"},
  "series": [
    {
      "name": "Sales",
      "type": "line",
      "data": [820, 932, 901, 934, 1290, 1330, 1320],
      "smooth": true
    },
    {
      "name": "Target",
      "type": "line",
      "data": [900, 900, 900, 900, 1200, 1200, 1200],
      "smooth": true
    }
  ]
}
```

---

## Bar Chart

Categorical comparison, including stacked bars.

```ec
{
  "tooltip": {"trigger": "axis", "axisPointer": {"type": "shadow"}},
  "legend": {"data": ["Direct", "Email", "Search"]},
  "grid": {"left": "3%", "right": "4%", "top": "8%", "bottom": "3%", "containLabel": true},
  "xAxis": {"type": "category", "data": ["Mon", "Tue", "Wed", "Thu", "Fri"]},
  "yAxis": {"type": "value"},
  "series": [
    {"name": "Direct", "type": "bar", "stack": "total", "data": [320, 332, 301, 334, 390]},
    {"name": "Email", "type": "bar", "stack": "total", "data": [120, 132, 101, 134, 90]},
    {"name": "Search", "type": "bar", "stack": "total", "data": [220, 182, 191, 234, 290]}
  ]
}
```

---

## Pie Chart

Proportional data, whole circle.

```ec
{
  "title": {"text": "Access Sources", "left": "center"},
  "tooltip": {"trigger": "item", "formatter": "{b}: {c} ({d}%)"},
  "legend": {"orient": "vertical", "left": "left"},
  "series": [
    {
      "name": "Access From",
      "type": "pie",
      "radius": "60%",
      "center": ["55%", "60%"],
      "data": [
        {"value": 1048, "name": "Search Engine"},
        {"value": 735, "name": "Direct"},
        {"value": 580, "name": "Email"},
        {"value": 484, "name": "Union Ads"},
        {"value": 300, "name": "Video Ads"}
      ],
      "emphasis": {"itemStyle": {"shadowBlur": 10, "shadowOffsetX": 0, "shadowColor": "rgba(0, 0, 0, 0.5)"}}
    }
  ]
}
```

---

## Donut Chart

Proportional data as a ring with a hollow center.

```ec
{
  "title": {"text": "Access Sources", "left": "center"},
  "tooltip": {"trigger": "item", "formatter": "{b}: {c} ({d}%)"},
  "legend": {"orient": "vertical", "left": "left"},
  "series": [
    {
      "name": "Access From",
      "type": "pie",
      "radius": ["40%", "70%"],
      "center": ["55%", "60%"],
      "avoidLabelOverlap": true,
      "itemStyle": {"borderRadius": 8, "borderColor": "#fff", "borderWidth": 2},
      "label": {"show": true, "formatter": "{b}\n{d}%"},
      "labelLine": {"show": true},
      "data": [
        {"value": 1048, "name": "Search Engine"},
        {"value": 735, "name": "Direct"},
        {"value": 580, "name": "Email"},
        {"value": 484, "name": "Union Ads"},
        {"value": 300, "name": "Video Ads"}
      ]
    }
  ]
}
```

---

## Scatter Plot

Distribution of two variables.

```ec
{
  "tooltip": {"trigger": "axis"},
  "xAxis": {"type": "value", "name": "Height (cm)", "min": 150, "max": 190},
  "yAxis": {"type": "value", "name": "Weight (kg)", "min": 40, "max": 100},
  "series": [
    {
      "type": "scatter",
      "symbolSize": 12,
      "data": [
        [152, 42], [171, 96], [158, 51], [184, 88],
        [155, 47], [177, 74], [162, 58], [188, 99],
        [165, 62], [181, 81], [170, 69], [159, 53]
      ]
    }
  ]
}
```

---

## Effect Scatter

Map-style scatter with rippling effects.

```ec
{
  "xAxis": {"type": "value", "name": "X"},
  "yAxis": {"type": "value", "name": "Y"},
  "series": [
    {
      "type": "effectScatter",
      "symbolSize": 20,
      "rippleEffect": {"scale": 4},
      "data": [
        {"value": [10, 20], "name": "Site A"},
        {"value": [30, 45], "name": "Site B"},
        {"value": [50, 70], "name": "Site C"},
        {"value": [80, 30], "name": "Site D"}
      ]
    }
  ]
}
```

---

## Radar Chart

Multidimensional comparison.

```ec
{
  "tooltip": {},
  "legend": {"data": ["Allocated Budget", "Actual Spending"]},
  "radar": {
    "indicator": [
      {"name": "Sales", "max": 6500},
      {"name": "Administration", "max": 16000},
      {"name": "Information Technology", "max": 30000},
      {"name": "Customer Support", "max": 38000},
      {"name": "Development", "max": 52000},
      {"name": "Marketing", "max": 25000}
    ]
  },
  "series": [
    {
      "name": "Budget vs Spending",
      "type": "radar",
      "data": [
        {"value": [4200, 3000, 20000, 35000, 50000, 18000], "name": "Allocated Budget"},
        {"value": [5000, 14000, 28000, 26000, 42000, 21000], "name": "Actual Spending"}
      ]
    }
  ]
}
```

---

## Box Plot

Statistical distribution of data.

```ec
{
  "tooltip": {"trigger": "item", "axisPointer": {"type": "shadow"}},
  "grid": {"left": "3%", "right": "4%", "top": "8%", "bottom": "3%", "containLabel": true},
  "xAxis": {"type": "category", "data": ["Class A", "Class B", "Class C"]},
  "yAxis": {"type": "value", "boundaryGap": [0, 0.01]},
  "series": [
    {
      "name": "Score Distribution",
      "type": "boxplot",
      "data": [
        [40, 56, 72, 88, 96],
        [32, 51, 68, 84, 95],
        [28, 46, 62, 80, 91]
      ]
    }
  ]
}
```

---

## Candlestick Stock Chart

Financial open-high-low-close data.

```ec
{
  "title": {"text": "Sample OHLC"},
  "tooltip": {"trigger": "axis", "axisPointer": {"type": "cross"}},
  "legend": {"data": ["OHLC", "MA5", "MA20"]},
  "grid": {"left": "5%", "right": "5%", "top": "8%", "bottom": "12%"},
  "xAxis": {
    "type": "category",
    "data": ["2026-06-01", "2026-06-02", "2026-06-03", "2026-06-04", "2026-06-05", "2026-06-08"],
    "boundaryGap": false
  },
  "yAxis": {"type": "value", "scale": true},
  "dataZoom": [
    {"type": "inside"},
    {"type": "slider", "bottom": 20, "height": 20}
  ],
  "series": [
    {
      "name": "OHLC",
      "type": "candlestick",
      "data": [
        [152.4, 153.9, 151.2, 154.8],
        [153.9, 154.6, 152.0, 155.1],
        [154.6, 156.2, 153.8, 156.7],
        [156.2, 154.7, 154.1, 156.9],
        [154.7, 153.4, 152.9, 155.3],
        [153.4, 152.1, 151.5, 154.2]
      ]
    },
    {
      "name": "MA5",
      "type": "line",
      "data": [null, null, null, null, 154.56, 154.2],
      "smooth": true,
      "showSymbol": false
    },
    {
      "name": "MA20",
      "type": "line",
      "data": [null, null, null, null, null, null],
      "smooth": true,
      "showSymbol": false
    }
  ]
}
```

---

## Heatmap

Matrix-like density of values on a grid.

```ec
{
  "tooltip": {"position": "top"},
  "grid": {"left": "3%", "right": "4%", "top": "8%", "bottom": "3%", "containLabel": true},
  "xAxis": {"type": "category", "data": ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]},
  "yAxis": {"type": "category", "data": ["Morning", "Afternoon", "Evening", "Night"]},
  "visualMap": {"min": 0, "max": 10, "calculable": true, "orient": "horizontal", "left": "center", "bottom": 0},
  "series": [
    {
      "name": "Activity",
      "type": "heatmap",
      "data": [
        [0, 0, 5], [1, 0, 7], [2, 0, 2], [3, 0, 4], [4, 0, 8], [5, 0, 9], [6, 0, 6],
        [0, 1, 4], [1, 1, 6], [2, 1, 3], [3, 1, 5], [4, 1, 7], [5, 1, 8], [6, 1, 5],
        [0, 2, 3], [1, 2, 4], [2, 2, 6], [3, 2, 8], [4, 2, 5], [5, 2, 3], [6, 2, 7],
        [0, 3, 2], [1, 3, 3], [2, 3, 5], [3, 3, 7], [4, 3, 4], [5, 3, 2], [6, 3, 6]
      ]
    }
  ]
}
```

---

## Sankey Flow

Energy or resource flows between nodes.

```ec
{
  "series": [
    {
      "type": "sankey",
      "data": [
        {"name": "a"}, {"name": "b"}, {"name": "a1"}, {"name": "b1"},
        {"name": "c"}, {"name": "e"}
      ],
      "links": [
        {"source": "a", "target": "a1", "value": 5},
        {"source": "e", "target": "b", "value": 3},
        {"source": "a", "target": "b1", "value": 3},
        {"source": "b1", "target": "a1", "value": 1},
        {"source": "b1", "target": "c", "value": 2},
        {"source": "b", "target": "c", "value": 1}
      ]
    }
  ]
}
```

---

## Treemap

Hierarchical data as nested rectangles.

```ec
{
  "series": [
    {
      "type": "treemap",
      "data": [
        {
          "name": "nodeA",
          "value": 10,
          "children": [
            {"name": "nodeAa", "value": 4},
            {"name": "nodeAb", "value": 6}
          ]
        },
        {
          "name": "nodeB",
          "value": 20,
          "children": [
            {"name": "nodeBa", "value": 12},
            {"name": "nodeBb", "value": 8}
          ]
        }
      ]
    }
  ]
}
```

---

## Sunburst

Radial hierarchy, an alternative to the treemap.

```ec
{
  "series": [
    {
      "type": "sunburst",
      "data": [
        {
          "name": "Root",
          "children": [
            {
              "name": "Sub A",
              "value": 5,
              "children": [
                {"name": "Leaf A1", "value": 2},
                {"name": "Leaf A2", "value": 3}
              ]
            },
            {
              "name": "Sub B",
              "value": 7,
              "children": [
                {"name": "Leaf B1", "value": 4},
                {"name": "Leaf B2", "value": 3}
              ]
            }
          ]
        }
      ],
      "radius": ["15%", "90%"],
      "label": {"rotate": 0}
    }
  ]
}
```

---

## Funnel

Progressive narrowing through stages.

```ec
{
  "tooltip": {"trigger": "item"},
  "legend": {"orient": "vertical", "left": "left"},
  "series": [
    {
      "name": "Conversion",
      "type": "funnel",
      "left": "15%",
      "top": 20,
      "bottom": 20,
      "width": "60%",
      "data": [
        {"value": 100, "name": "Visited"},
        {"value": 80, "name": "Consulted"},
        {"value": 60, "name": "Purchased"},
        {"value": 30, "name": "Retained"}
      ]
    }
  ]
}
```

---

## Gauge

Single-value indicator on a dial.

```ec
{
  "series": [
    {
      "type": "gauge",
      "min": 0,
      "max": 220,
      "progress": {"show": true},
      "axisLine": {"lineStyle": {"width": 18}},
      "pointer": {"length": "60%"},
      "detail": {"formatter": "{value} km/h"},
      "data": [{"value": 168, "name": "Speed"}]
    }
  ]
}
```

---

## Graph

Nodes and edges for networks, force-directed when laid out.

```ec
{
  "tooltip": {},
  "legend": [{"data": ["a", "b"]}],
  "series": [
    {
      "type": "graph",
      "layout": "force",
      "roam": true,
      "symbolSize": 60,
      "edgeLength": [200, 300],
      "label": {"show": true, "fontSize": 18},
      "force": {"repulsion": 600, "edgeLength": 250},
      "data": [
        {"name": "N1", "value": 10},
        {"name": "N2", "value": 20},
        {"name": "N3", "value": 30},
        {"name": "N4", "value": 40}
      ],
      "links": [
        {"source": "N1", "target": "N2"},
        {"source": "N2", "target": "N3"},
        {"source": "N3", "target": "N4"},
        {"source": "N1", "target": "N4"}
      ]
    }
  ]
}
```

---

## Parallel

Compare many variables across several dimensions.

```ec
{
  "parallelAxis": [
    {"dim": 0, "name": "Dim 0"},
    {"dim": 1, "name": "Dim 1"},
    {"dim": 2, "name": "Dim 2"},
    {"dim": 3, "name": "Dim 3"}
  ],
  "parallel": {"top": "15%", "left": "10%", "right": "10%", "bottom": "10%"},
  "series": [
    {
      "type": "parallel",
      "lineStyle": {"width": 1, "opacity": 0.5},
      "data": [
        [1, 3, 2, 4],
        [2, 4, 1, 3],
        [3, 2, 4, 1],
        [4, 1, 3, 2],
        [2, 3, 1, 4]
      ]
    }
  ]
}
```

---

## Pictorial Bar

Bars replaced by repeated symbols.

```ec
{
  "tooltip": {},
  "grid": {"left": "3%", "right": "4%", "bottom": "3%", "containLabel": true},
  "xAxis": {"type": "category", "data": ["Mon", "Tue", "Wed", "Thu", "Fri"]},
  "yAxis": {"type": "value"},
  "series": [
    {
      "name": "Score",
      "type": "pictorialBar",
      "symbol": "rect",
      "symbolRepeat": true,
      "symbolSize": [12, 16],
      "symbolOffset": [0, 12],
      "barWidth": "50%",
      "itemStyle": {"color": "#5470c6"},
      "data": [60, 80, 50, 90, 70]
    }
  ]
}
```

---

## ThemeRiver

Evolution of multiple categories over time.

```ec
{
  "tooltip": {"trigger": "axis"},
  "singleAxis": {"type": "time", "top": "5%", "bottom": "5%"},
  "series": [
    {
      "type": "themeRiver",
      "data": [
        ["2023-01-01", 5, "Apple"],
        ["2023-01-02", 6, "Apple"],
        ["2023-01-03", 4, "Apple"],
        ["2023-01-01", 3, "Banana"],
        ["2023-01-02", 5, "Banana"],
        ["2023-01-03", 6, "Banana"]
      ]
    }
  ]
}
```

---

## Calendar

Date-scoped values mapped on a heatmap calendar.

```ec
{
  "tooltip": {},
  "visualMap": {"min": 0, "max": 10, "calculable": false, "orient": "horizontal", "bottom": 20},
  "calendar": {
    "range": "2026-07",
    "top": 50,
    "left": 60,
    "cellSize": ["auto", 18]
  },
  "series": [
    {
      "type": "heatmap",
      "coordinateSystem": "calendar",
      "data": [
        ["2026-07-01", 1], ["2026-07-02", 4], ["2026-07-03", 7], ["2026-07-04", 2],
        ["2026-07-05", 9], ["2026-07-06", 3], ["2026-07-07", 5], ["2026-07-08", 8]
      ]
    }
  ]
}
```

---

## Styled Example

Multiple series and coordinate features combined.

```ec
{
  "title": {"text": "Combined Styled Chart"},
  "tooltip": {"trigger": "axis"},
  "legend": {"data": ["Revenue", "Growth"]},
  "toolbox": {"feature": {"saveAsImage": {}, "dataZoom": {}, "restore": {}}},
  "grid": {"left": "3%", "right": "4%", "top": "10%", "bottom": "3%", "containLabel": true},
  "xAxis": {"type": "category", "data": ["Q1", "Q2", "Q3", "Q4"]},
  "yAxis": [
    {"type": "value", "name": "Revenue"},
    {"type": "value", "name": "Growth", "max": 100}
  ],
  "series": [
    {"name": "Revenue", "type": "bar", "data": [120, 200, 150, 280]},
    {"name": "Growth", "type": "line", "yAxisIndex": 1, "data": [15, 40, 25, 60], "smooth": true}
  ]
}
```
