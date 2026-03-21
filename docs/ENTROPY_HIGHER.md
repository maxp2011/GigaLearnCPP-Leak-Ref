# Why “Policy Entropy” looked higher than Zealan / GGL

## Root cause (code) — **fixed**

1. **`InferPolicyProbsFromModels`** used `clamp(min_prob)` on **every** action dimension, then **renormalized over all dims**.  
   Masked (invalid) actions were forced to carry tiny probability. That **spreads mass** onto invalid actions and **raises** Shannon entropy vs a true masked-softmax policy.

2. **`ComputeEntropy`** used `probs.clamp_min(1e-10).log()` on **all** dimensions. For invalid actions with `p=0`, that turns into `1e-10 * log(1e-10)` — **non-zero fake** contribution and again **higher** entropy.

3. **`InferActionsFromModels`** applied another **global** `clamp_min` + renormalize — same inflation if present.

After the fix: probabilities stay **supported on valid actions only**; entropy uses `p*log(p)` with **zero** contribution where `p==0`.

## Other reasons entropy can still differ (not bugs)

| Factor | Effect |
|--------|--------|
| **`maskEntropy` false** | Reported value is **normalized** by `log(full_action_count)` (see `PPOLearnerConfig`). Compare runs with the **same** flag. |
| **`maskEntropy` true** | Normalizes by `log(# valid actions per row)` — often **lower** reported number when many actions are masked. |
| **Obs / rewards** | `CustomObs`, big nets, and your reward mix change **what** the policy learns → different logits → different entropy. |
| **`entropyScale` / LR** | Stronger entropy bonus or training dynamics can keep policies **more stochastic** longer. |
| **Temperature** | `policyTemperature != 1` scales logits before softmax. |

## What to compare fairly

- Same **`cfg.ppo.maskEntropy`**
- Same **obs builder** (or accept that CustomObs ≠ AdvancedObs)
- Same **architecture** if you want metrics to match another project

## Reference

- SB3 / PPO “policy entropy” is usually the **differential** entropy of the **categorical** over **valid** actions when masking is used correctly.
