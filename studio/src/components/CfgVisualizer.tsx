import React, { useMemo, useState } from "react";
import { BasicBlockNode, FunctionAnalysis } from "../types";

interface Props {
  functions: FunctionAnalysis[];
}

export const CfgVisualizer: React.FC<Props> = ({ functions }) => {
  const [selectedIdx, setSelectedIdx] = useState<number>(0);
  const [selectedBlock, setSelectedBlock] = useState<BasicBlockNode | null>(null);

  const fn = functions[selectedIdx] ?? functions[0];

  const layout = useMemo(() => {
    if (!fn) return { nodes: [], width: 700, height: 320 };
    const n = fn.blocks.length;
    const depth = new Array<number>(n).fill(0);

    // Longest-path DAG layering ignoring back-edges
    for (let iter = 0; iter < n; iter++) {
      for (const e of fn.edges) {
        if (!e.is_backedge && e.src < n && e.dst < n) {
          if (depth[e.dst] < depth[e.src] + 1) {
            depth[e.dst] = depth[e.src] + 1;
          }
        }
      }
    }

    const layers = new Map<number, BasicBlockNode[]>();
    fn.blocks.forEach((b, idx) => {
      const d = depth[idx];
      const list = layers.get(d) ?? [];
      list.push(b);
      layers.set(d, list);
    });

    const maxLayer = Math.max(0, ...Array.from(layers.keys()));
    const maxCols = Math.max(1, ...Array.from(layers.values()).map((l) => l.length));
    const colWidth = 175;
    const rowHeight = 96;
    const width = Math.max(680, maxCols * colWidth + 60);
    const height = Math.max(300, (maxLayer + 1) * rowHeight + 60);

    const nodes: Array<{ block: BasicBlockNode; x: number; y: number }> = [];
    for (let d = 0; d <= maxLayer; d++) {
      const row = layers.get(d) ?? [];
      const rowSpan = row.length * colWidth;
      const startX = (width - rowSpan) / 2 + colWidth / 2;
      row.forEach((blk, cIdx) => {
        nodes.push({
          block: blk,
          x: startX + cIdx * colWidth,
          y: 48 + d * rowHeight
        });
      });
    }

    return { nodes, width, height };
  }, [fn]);

  if (!fn) {
    return <div className="panel-body">No CFG functions available.</div>;
  }

  const posById = new Map<number, { x: number; y: number }>();
  layout.nodes.forEach((n) => posById.set(n.block.id, { x: n.x, y: n.y }));

  return (
    <div className="panel">
      <div className="panel-header">
        <span>LLVM IR CONTROL FLOW GRAPH &amp; WORST-CASE EXECUTION PATH</span>
        <span>
          WCET: {fn.wcet_cycles} CYCLES
          {fn.infeasible_path_savings_cycles > 0
            ? ` (-${fn.infeasible_path_savings_cycles} MUTEX PRUNED)`
            : ""}
        </span>
      </div>
      <div className="panel-body">
        <div className="fn-tabs">
          {functions.map((f, idx) => (
            <button
              key={f.mangled_name + idx}
              className={`fn-tab ${idx === selectedIdx ? "fn-tab-active" : ""}`}
              onClick={() => {
                setSelectedIdx(idx);
                setSelectedBlock(null);
              }}
            >
              {f.demangled_name.replace(/^secu::/, "").replace(/\(.*\)$/, "()")} [
              {f.provable ? `${f.wcet_cycles}c` : "UNBOUNDED"}]
            </button>
          ))}
        </div>

        <div className="metrics-strip">
          <span>
            <strong>STATUS:</strong> {fn.solver_status}
          </span>
          <span>
            <strong>UNPRUNED:</strong> {fn.unpruned_wcet_cycles} cycles
          </span>
          <span>
            <strong>PRUNED WCET:</strong> {fn.wcet_cycles} cycles
          </span>
          <span>
            <strong>LOOPS:</strong>{" "}
            {fn.loops.length === 0
              ? "None"
              : fn.loops
                  .map((l) => `L${l.loop_id}: ${l.bounded ? `${l.max_iterations}x (${l.bound_source})` : "UNBOUNDED"}`)
                  .join(", ")}
          </span>
        </div>

        <div style={{ overflowX: "auto", border: "1px solid #c8c8c0", background: "#fafaf8" }}>
          <svg width={layout.width} height={layout.height}>
            <defs>
              <marker id="arrow-gray" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto">
                <path d="M0,0 L0,6 L8,3 z" fill="#6b7280" />
              </marker>
              <marker id="arrow-red" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto">
                <path d="M0,0 L0,6 L8,3 z" fill="#dc2626" />
              </marker>
            </defs>

            {fn.edges.map((e, idx) => {
              const s = posById.get(e.src);
              const t = posById.get(e.dst);
              if (!s || !t) return null;

              const isHot = e.on_wcep;
              const stroke = isHot ? "#dc2626" : "#9ca3af";
              const strokeWidth = isHot ? 2.5 : 1.2;

              if (e.is_backedge) {
                const curveX = Math.max(s.x, t.x) + 88;
                const dPath = `M ${s.x + 68} ${s.y} C ${curveX} ${s.y}, ${curveX} ${t.y}, ${t.x + 68} ${t.y}`;
                return (
                  <g key={`edge-${idx}`}>
                    <path
                      d={dPath}
                      fill="none"
                      stroke={stroke}
                      strokeWidth={strokeWidth}
                      strokeDasharray="4 3"
                      markerEnd={isHot ? "url(#arrow-red)" : "url(#arrow-gray)"}
                    />
                    <text
                      x={curveX - 6}
                      y={(s.y + t.y) / 2}
                      fontSize="10"
                      fontFamily="monospace"
                      fill={isHot ? "#dc2626" : "#4b5563"}
                      fontWeight="bold"
                    >
                      back x{e.flow_count}
                    </text>
                  </g>
                );
              }

              return (
                <g key={`edge-${idx}`}>
                  <line
                    x1={s.x}
                    y1={s.y + 24}
                    x2={t.x}
                    y2={t.y - 24}
                    stroke={stroke}
                    strokeWidth={strokeWidth}
                    markerEnd={isHot ? "url(#arrow-red)" : "url(#arrow-gray)"}
                  />
                  {e.flow_count > 0 && (
                    <text
                      x={(s.x + t.x) / 2 + 5}
                      y={(s.y + t.y) / 2}
                      fontSize="10"
                      fontFamily="monospace"
                      fill="#dc2626"
                      fontWeight="bold"
                    >
                      x{e.flow_count}
                    </text>
                  )}
                </g>
              );
            })}

            {layout.nodes.map(({ block, x, y }) => {
              const isHot = block.on_wcep;
              const cacheColor =
                block.cache_class === "AH"
                  ? "#047857"
                  : block.cache_class === "FM"
                  ? "#d97706"
                  : "#dc2626";

              return (
                <g
                  key={`bb-${block.id}`}
                  transform={`translate(${x - 72}, ${y - 24})`}
                  style={{ cursor: "pointer" }}
                  onClick={() => setSelectedBlock(block)}
                >
                  <rect
                    width="144"
                    height="48"
                    fill={isHot ? "#fee2e2" : "#ffffff"}
                    stroke={isHot ? "#dc2626" : "#1c1f26"}
                    strokeWidth={isHot ? 2.2 : 1.2}
                  />
                  <rect width="28" height="16" x="116" y="0" fill={cacheColor} />
                  <text
                    x="130"
                    y="11"
                    textAnchor="middle"
                    fill="#ffffff"
                    fontSize="10"
                    fontFamily="monospace"
                    fontWeight="bold"
                  >
                    {block.cache_class}
                  </text>
                  <text
                    x="6"
                    y="14"
                    fill="#14161a"
                    fontSize="10"
                    fontFamily="monospace"
                    fontWeight="bold"
                  >
                    BB{block.id}: {block.label.slice(0, 12)}
                  </text>
                  <text x="6" y="29" fill="#374151" fontSize="10" fontFamily="monospace">
                    {block.first_iter_cycles}/{block.steady_iter_cycles}c · exec={block.exec_count}
                  </text>
                  <text x="6" y="42" fill={isHot ? "#b91c1c" : "#6b7280"} fontSize="10" fontFamily="monospace">
                    sum={block.total_cycles_contributed}c {block.mutex_tag ? `[${block.mutex_tag}]` : ""}
                  </text>
                </g>
              );
            })}
          </svg>
        </div>

        {selectedBlock && (
          <div className="metrics-strip" style={{ marginTop: "8px", marginBottom: 0 }}>
            <span>
              <strong>SELECTED BB{selectedBlock.id}:</strong> {selectedBlock.label}
            </span>
            <span>
              <strong>I-CACHE:</strong> {selectedBlock.cache_class}
            </span>
            <span>
              <strong>1ST / STEADY COST:</strong> {selectedBlock.first_iter_cycles} /{" "}
              {selectedBlock.steady_iter_cycles} cycles
            </span>
            <span>
              <strong>ILP EXEC COUNT:</strong> {selectedBlock.exec_count} (total{" "}
              {selectedBlock.total_cycles_contributed} cycles)
            </span>
          </div>
        )}
      </div>
    </div>
  );
};
