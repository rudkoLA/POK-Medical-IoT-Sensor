import React, { ReactNode, useMemo, useState } from "react";
import { View, Text, LayoutChangeEvent, StyleSheet } from "react-native";
import Svg, { Path, Line, G, Text as SvgText, Circle } from "react-native-svg";

type Size = { width: number; height: number };

type GraphProps = {
  name: string;
  data: number[];
  baseMin: number;
  baseMax: number;
  labelCount: number;
  maxPoints: number;
  title: string;
  accentColor: string;
  icon: ReactNode;
  dataUnit: string;
  loading: boolean;
};

const Graph = ({
  name,
  data,
  baseMin,
  baseMax,
  labelCount,
  maxPoints,
  title,
  dataUnit,
  accentColor,
  icon,
  loading,
}: GraphProps) => {
  const [size, setSize] = useState<Size>({ width: 0, height: 0 });
  const [min, setMin] = useState<number>(baseMin);
  const [max, setMax] = useState<number>(baseMax);

  const onLayout = (e: LayoutChangeEvent) => {
    const { width, height } = e.nativeEvent.layout;
    setSize({ width, height });
  };

  const chart = useMemo(() => {
    const w = Math.max(0, size.width - 56);
    const h = Math.max(0, size.height - 32);
    if (w <= 0 || h <= 0 || data.length === 0) return null;

    const points = data.slice(-maxPoints);
    const stepX = points.length === 1 ? 0 : w / (maxPoints - 1);

    setMin(Math.min(baseMin, Math.min(...points)));
    setMax(Math.max(baseMax, Math.max(...points)));

    const mapped = points.map((v, i) => {
      const x = i * stepX;
      const t = (v - min) / (max - min);
      const y = h - Math.max(0, Math.min(1, t)) * h;

      return { x, y, v };
    });

    let d = "";

    for (let i = 0; i < mapped.length; i++) {
      const p = mapped[i];

      if (i === 0) {
        d += `M${p.x.toFixed(2)},${p.y.toFixed(2)} `;
      } else {
        const prev = mapped[i - 1];

        const cx1 = prev.x + (p.x - prev.x) / 1.8;
        const cy1 = prev.y;

        const cx2 = p.x - (p.x - prev.x) / 1.8;
        const cy2 = p.y;

        d += `C${cx1.toFixed(2)},${cy1.toFixed(2)} ${cx2.toFixed(
          2
        )},${cy2.toFixed(2)} ${p.x.toFixed(2)},${p.y.toFixed(2)} `;
      }
    }

    return { width: w, height: h, mapped, d };
  }, [size, data]);

  const lastValue = Math.round(data.at(-1) ?? NaN);

  return (
    <View style={[styles.container, { backgroundColor: accentColor }]}>
      <View style={[styles.headerRow, {}]}>
        <Text style={styles.title}>
          {title}:{" "}
          {isNaN(lastValue) || loading ? "--" : lastValue + " " + dataUnit}
        </Text>
        {icon}
      </View>

      <View style={styles.chartWrap} onLayout={onLayout}>
        {chart && (
          <Svg width={size.width} height={size.height}>
            <G>
              {chart &&
                (() => {
                  const labels = Array.from(
                    { length: labelCount },
                    (_, i) => min + i * ((max - min) / (labelCount - 1))
                  );

                  return labels.map((val) => {
                    const t = (val - min) / (max - min);
                    const y = chart.height - t * chart.height;

                    return (
                      <React.Fragment key={`fragment-${name}-${val}`}>
                        <Line
                          key={`h-line-${name}-${val}`}
                          x1={36}
                          y1={y + 16}
                          x2={size.width}
                          y2={y + 16}
                          stroke={accentColor}
                          strokeOpacity={0.8}
                          strokeWidth={2}
                        />
                        <SvgText
                          key={`label-${name}-${val}`}
                          x={28}
                          y={y + 20}
                          fontSize={12}
                          fill="#fff"
                          textAnchor="end"
                        >
                          {Math.round(val)}
                        </SvgText>
                      </React.Fragment>
                    );
                  });
                })()}
            </G>

            <G x={40} y={16}>
              <Path
                d={chart.d}
                fill="none"
                stroke="#fff"
                strokeWidth={2}
                strokeLinejoin="round"
                strokeLinecap="round"
              />

              {chart.mapped.length > 0 && (
                <Circle
                  cx={chart.mapped.at(-1)!.x}
                  cy={chart.mapped.at(-1)!.y}
                  r={4}
                  fill="#fff"
                />
              )}
            </G>
          </Svg>
        )}
      </View>
      {loading && (
        <View
          style={[
            StyleSheet.absoluteFillObject,
            { backgroundColor: "#00000033" },
          ]}
        ></View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    width: "100%",
    borderRadius: 16,
    padding: 2,
  },
  headerRow: {
    width: "100%",
    display: "flex",
    flexDirection: "row",
    justifyContent: "space-between",
    paddingHorizontal: 8,
    marginBottom: 8,
    alignItems: "center",
  },
  title: {
    color: "#fff",
    fontSize: 24,
  },
  chartWrap: {
    height: 256,
    width: "100%",
    backgroundColor: "#000",
    borderRadius: 16,
    overflow: "hidden",
  },
});

export default Graph;
