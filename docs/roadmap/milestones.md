# Product Milestones

The sequence below communicates dependency order, not fixed versions or dates.
Each milestone is not started unless represented in
[current priorities](current.md).

| Milestone | Outcome |
| --- | --- |
| Composition stabilization | The Windows reference runtime has stable metadata, evidence, naming, and reconstruction contracts. |
| Vector and portability groundwork | Vector capabilities are composed and target-aware layout supports a second platform. |
| C++ SDK | A small tested native API exposes open, inspection, introspection, and structured diagnostics. |
| Python distribution | Python 3.13 users can install, import, and open through a measured runtime-delivery model. |
| Node distribution | An ESM-first package exposes the stabilized conceptual SDK through a measured native delivery model. |
| Wasm experiment | A portable read-oriented subset proves inspection, detection, metadata, and small-input behavior. |

Platform support expands one evidence-backed target at a time: Linux x86_64
after Windows, then macOS arm64 with its additional binary distribution and
code-signing requirements.
