import React, { useRef, useEffect, useState } from "react";
import { Button, Select, MenuItem } from "@mui/material";

type RegionName = "Region 1" | "Region 2" | "Region 3" | "Region 4" | "Region 5";

const regionLines: Record<RegionName, number[][]> = {
  "Region 1": [
    [160, 230, 110, 310, 160, 320], // end x, y, x curve start, y curve start, x curve end, y curve end, start x, y
  ],
  "Region 2": [
    [160, 230, 200, 230, 255, 180],
  ],
  "Region 3": [
    [255, 180, 275, 140, 300, 145],
  ],
  "Region 4": [
    [300, 145, 400, 150, 390, 120],
    [390, 120, 440, 110, 440, 100],
  ],
  "Region 5": [
    [440, 100, 415, 95, 420, 50],
    [420, 70, 430, 170, 540, 105],
  ],
};

const regionColors: Record<RegionName, string> = {
  "Region 1": "red",
  "Region 2": "green",
  "Region 3": "yellow",
  "Region 4": "blue",
  "Region 5": "purple",
};

const CarpAnimationGUI: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const pausedRef = useRef(false);
  const [manualData, setManualData] = useState<Record<number, Record<RegionName, boolean>>>(() => {
    const initialData: Record<number, Record<RegionName, boolean>> = {};
    for (let year = 2016; year <= 2025; year++) {
      initialData[year] = {
        "Region 1": false,
        "Region 2": false,
        "Region 3": false,
        "Region 4": false,
        "Region 5": false,
      };
    }
    return initialData;
  });

  const pathsRef = useRef<Record<RegionName, Path2D[]>>({
    "Region 1": [],
    "Region 2": [],
    "Region 3": [],
    "Region 4": [],
    "Region 5": [],
  });

  const animationRef = useRef<number | null>(null);
  const frameRef = useRef(0);
  const lastFrameTimeRef = useRef(0);

  const baseImage = new Image();
  baseImage.src = "/River.png";

  useEffect(() => {
    const ctx = canvasRef.current?.getContext("2d");
    if (!ctx) return;

    const newPaths: Record<RegionName, Path2D[]> = {
      "Region 1": [],
      "Region 2": [],
      "Region 3": [],
      "Region 4": [],
      "Region 5": [],
    };

    Object.entries(regionLines).forEach(([regionName, segments]) => {
      const region = regionName as RegionName;
      const paths: Path2D[] = [];

      if (segments.length === 0) return;

      let [startX, startY] = segments[0].slice(0, 2);

      segments.forEach(segment => {
        if (segment.length === 6) {
          const [cp1x, cp1y, cp2x, cp2y, x, y] = segment;
          const path = new Path2D();
          path.moveTo(startX, startY);
          path.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
          paths.push(path);
          startX = x;
          startY = y;
        }
      });

      newPaths[region] = paths;
    });

    pathsRef.current = newPaths;
  }, []);

  const drawAnimation = (timestamp: number, structuredData: Record<number, Record<RegionName, boolean>>) => {
    const ctx = canvasRef.current?.getContext("2d");
    if (!ctx) return;

    const years = Object.keys(structuredData).sort();

    if (timestamp - lastFrameTimeRef.current >= 1000) {
      ctx.clearRect(0, 0, 640, 480);
      ctx.drawImage(baseImage, 0, 0, 640, 480);

      const year = years[frameRef.current % years.length];
      const regionData = structuredData[parseInt(year)];

      (Object.entries(regionData) as [RegionName, boolean][]).forEach(([regionName, isActive]) => {
        const color = regionColors[regionName];
        const paths = pathsRef.current[regionName];
        if (!paths) return;

        ctx.strokeStyle = isActive ? color : "gray";
        ctx.lineWidth = 4;
        paths.forEach(path => {
          ctx.stroke(path);
        });
      });

      
      const legendX = 480;  
      const legendYStart = 350;  
      let legendY = legendYStart;

      Object.entries(regionColors).forEach(([regionName, color]) => {
        ctx.fillStyle = color;
        ctx.fillRect(legendX, legendY, 15, 15);
        ctx.fillStyle = "black";
        ctx.fillText(regionName, legendX + 20, legendY + 12);
        legendY += 25;
      });

      ctx.fillStyle = "black";
      ctx.font = "20px Arial";
      ctx.fillText(`Year: ${year}`, 20, 30);

      lastFrameTimeRef.current = timestamp;
      frameRef.current++;
    }

    if (!pausedRef.current) {
      animationRef.current = requestAnimationFrame((t) => drawAnimation(t, structuredData));
    }
  };

  const startAnimation = (structuredData: Record<number, Record<RegionName, boolean>>) => {
    cancelAnimationFrame(animationRef.current!);
    frameRef.current = 0;
    lastFrameTimeRef.current = 0;
    pausedRef.current = false;
    animationRef.current = requestAnimationFrame((timestamp) => drawAnimation(timestamp, structuredData));
  };

  const resumeAnimation = () => {
    if (pausedRef.current) {
      pausedRef.current = false;
      animationRef.current = requestAnimationFrame((timestamp) => drawAnimation(timestamp, manualData));
    }
  };

  const pauseAnimation = () => {
    pausedRef.current = true;
    cancelAnimationFrame(animationRef.current!);
  };

  return (
    <div style={{ textAlign: "center" }}>
      <h2>Carp Animation GUI</h2>
      <div style={{ position: "relative", marginBottom: "20px" }}>
        <canvas ref={canvasRef} width="640" height="480" style={{ border: "1px solid black" }} />
      </div>
      <div style={{ marginTop: "20px" }}>
        <Button
          variant="contained"
          onClick={() => {
            startAnimation(manualData);
          }}
          style={{ marginRight: "10px" }}
        >
          Run Animation with Manual Data
        </Button>
        <Button
          variant="contained"
          color="warning"
          onClick={pauseAnimation}
          style={{ marginRight: "10px" }}
        >
          Pause
        </Button>
        <Button
          variant="contained"
          color="success"
          onClick={resumeAnimation}
        >
          Resume
        </Button>
      </div>
      <div style={{ marginTop: "40px" }}>
        <h3>Manual Entry Table (2016-2025)</h3>
        <table style={{ margin: "0 auto", borderCollapse: "collapse" }}>
          <thead>
            <tr>
              <th>Year</th>
              {["Region 1", "Region 2", "Region 3", "Region 4", "Region 5"].map(region => (
                <th key={region}>{region}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {Object.entries(manualData).map(([yearStr, values]) => (
                <tr key={yearStr}>
                <td>{yearStr}</td>
                {Object.entries(values).map(([region, val]) => (
                  <td key={region}>
                  <input
                    type="checkbox"
                    checked={val}
                    onChange={(e) => {
                    const newVal = e.target.checked;
                    setManualData(prev => ({
                      ...prev,
                      [yearStr]: {
                      ...prev[+yearStr],
                      [region]: newVal,
                      },
                    }));
                    }}
                  />
                  </td>
                ))}
                </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default CarpAnimationGUI;
