# Learning setup: this repo vs `PPOLearnerConfig` defaults

The **PPO math** (GAE, clipped surrogate, entropy term, two shared-head forwards in `Learn`, etc.) lives in `GigaLearnCPP` and matches the normal GigaLearnCPP stack.

What differs in **this** project is mostly **`src/ExampleMain.cpp`**, which overrides `cfg.ppo.*` for large nets and long runs.

## `PPOLearnerConfig` defaults (`GigaLearnCPP/.../PPOLearnerConfig.h`)

| Field | Default |
|--------|--------|
| `tsPerItr` | 50,000 |
| `epochs` | 2 |
| `policyLR` / `criticLR` | 3e-4 |
| `entropyScale` | 0.018 |
| `maskEntropy` | false |
| `policyTemperature` | 1.0 |
| `gaeGamma` | 0.99 |
| `gaeLambda` | 0.95 |
| `clipRange` | 0.2 |
| `rewardClipRange` | 10 |

## Typical overrides in `ExampleMain.cpp` (this repo)

| Field | Typical value here | Why |
|--------|-------------------|-----|
| `tsPerItr` / `batchSize` | 1,000,000 | Large parallel env (vs default 50k) |
| `epochs` | **2** | Matches stock `PPOLearnerConfig` / GGL |
| `policyLR` / `criticLR` | **3e-4** | Same as `PPOLearnerConfig` / rlgym-ppo baseline |
| `entropyScale` | **0.018** | Same as GGL default (not 0.035) |
| `maskEntropy` | **false** | Same as GGL default |
| `gaeGamma` / `gaeLambda` | **0.99 / 0.95** | Explicit; matches defaults |
| `miniBatchSize` | 25,000 | GPU minibatches (default 0 = full batch would be huge) |

Architecture (2048² shared, 1024/512/512, LEAKY_RELU) stays **larger** than the small MLP in Zealan’s sample `ExampleMain.cpp`; only the **PPO optimization** side is aligned with stock GigaLearnCPP so 2v2/TL behavior is closer to “normal” GGL.

**Still using defaults from `PPOLearnerConfig`:** `clipRange`, `rewardClipRange`, `policyTemperature`, etc., unless you override them elsewhere.

## Older experimental preset (not GGL-default)

If you were using: `entropyScale` 0.035, `maskEntropy` true, `gaeGamma` 0.9955, LR 12e-5 — that diverges from stock GGL and can change exploration vs policy updates a lot (often higher entropy / different update scale in 2v2).

## Files that affect learning correctness (not hyperparams)

- `GigaLearnCPP/.../PPO/GAE.cpp` — bootstrap at last timestep
- `GigaLearnCPP/.../PPO/PPOLearner.cpp` — PPO update, grad norm
- `GigaLearnCPP/.../Learner.cpp` — rollout collection, GAE call, experience → `Learn`
