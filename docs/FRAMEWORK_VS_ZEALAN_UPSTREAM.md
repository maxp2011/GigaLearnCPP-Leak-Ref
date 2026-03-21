# Framework “blood” vs [ZealanL/GigaLearnCPP-Leak](https://github.com/ZealanL/GigaLearnCPP-Leak)

This doc compares **core learning code** in **this repo** to a shallow clone of Zealan’s upstream (`main`). Use it to see what is **intentionally identical**, **bugfixed**, or **environment / UX**.

## How to re-diff locally

```bash
git clone --depth 1 https://github.com/ZealanL/GigaLearnCPP-Leak.git _upstream_GigaLearnCPP-Leak
git diff --no-index _upstream_GigaLearnCPP-Leak/GigaLearnCPP/src/private/GigaLearnCPP/PPO/PPOLearner.cpp \
  GigaLearnCPP-Leak-Ref/GigaLearnCPP/src/private/GigaLearnCPP/PPO/PPOLearner.cpp
# repeat for GAE.cpp, Learner.cpp, etc.
```

## `PPOLearner.cpp`

| Area | Zealan upstream | This repo |
|------|-----------------|-----------|
| **`Learn()`** | `InferPolicyProbsFromModels(..., false)` + `InferCritic(obs)`; shared head runs **twice** per minibatch (policy path then critic path), same as upstream design | **Restored to match upstream**; minibatch scaling uses **`curBatchSize`** (actual tensor length) instead of always `config.batchSize` — fixes wrong `batchSizeRatio` / OOB slices on the **last** batch when overbatching produces a short batch |
| **`InferPolicyProbsFromModels`** | Softmax + clamp | Extra **renormalize** + `finite` guard so `multinomial` / training doesn’t hit NaN |
| **`InferActionsFromModels`** | Direct `multinomial` | Clamp + renormalize before `multinomial` (CUDA assert guard) |
| **`ComputeEntropy`** | `probs.log()` (can be `-inf` at 0) | `clamp_min` on probs before log; safer **maskEntropy** denominator |
| **`TransferLearn`** | Baseline | Optional **finite** guards on `oldProbs` / `newProbs` for KL stability |

**Note:** In upstream, policy forward in `Learn` uses `halfPrec = false`, while `InferCritic` uses `config.useHalfPrecision` for shared + critic. That asymmetry is **unchanged** — it matches the leak.

## `GAE.cpp`

| Area | Zealan upstream | This repo |
|------|-----------------|-----------|
| Last step `nextValPred` | Reads `_valPreds[step + 1]` even when `step + 1 == numReturns` (**undefined behavior**) | **Fixed:** only read `step+1` when in range; else `0` (terminal mask zeros contribution) |
| `returnStd` | Used as-is | **Guard** invalid / non-finite → `1.f` |
| Tensors | Raw | **Finite** sanitization on rewards / value preds |

These are **correctness / stability** fixes; they do not change the intended GAE formula when data is well-behaved.

## `Learner.cpp`

Upstream vs this repo differs mainly in **runtime robustness** (Python path, checkpoints without stats, NaN-safe obs norm, **reward metric index** fix, rollout logging). The **PPO call sequence** (collect → value preds → GAE → `ppo->Learn`) is the same structure; local changes fix freezes, bad metrics, and edge-case tensors.

## `PPOLearnerConfig.h`

**No meaningful diff** vs upstream — defaults are the stock GigaLearnCPP leak defaults.

## Summary

- **PPO math inside `Learn()`** is aligned with **Zealan’s leak** again (`InferPolicyProbs*` + `InferCritic`), plus the **`curBatchSize`** fix for real batch shapes.
- **Defensive** inference / entropy / GAE / Learner fixes remain so training doesn’t die on NaNs or UB while behaving like upstream when inputs are clean.
