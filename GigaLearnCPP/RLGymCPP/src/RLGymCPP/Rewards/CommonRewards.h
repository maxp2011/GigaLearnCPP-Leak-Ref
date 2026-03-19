#pragma once
#include "Reward.h"
#include "../Math.h"

namespace RLGC {

	using namespace CommonValues;

	inline float distance2D(float x1, float z1, float x2, float z2) {
		float dx = x2 - x1;
		float dz = z2 - z1;
		return std::sqrt(dx * dx + dz * dz);
	}

	inline std::vector<float> linspace(float start_in, float end_in, int num_in) {
		std::vector<float> linspaced;

		if (num_in <= 0) {
			return linspaced;
		}
		if (num_in == 1) {
			linspaced.push_back(start_in);
			return linspaced;
		}

		float delta = (end_in - start_in) / (num_in - 1);
		for (int i = 0; i < num_in; ++i) {
			linspaced.push_back(start_in + delta * i);
		}
		return linspaced;
	}

	// ============================================================================
	// EVENT-BASED REWARDS (template elegante - manteniamo invariato)
	// ============================================================================

	template<bool PlayerEventState::* VAR, bool NEGATIVE>
	class PlayerDataEventReward : public Reward {
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			bool val = player.eventState.*VAR;

			if (NEGATIVE) {
				return -(float)val;
			}
			else {
				return (float)val;
			}
		}
	};

	typedef PlayerDataEventReward<&PlayerEventState::goal, false> PlayerGoalReward;
	typedef PlayerDataEventReward<&PlayerEventState::assist, false> AssistReward;
	typedef PlayerDataEventReward<&PlayerEventState::shot, false> ShotReward;
	typedef PlayerDataEventReward<&PlayerEventState::shotPass, false> ShotPassReward;
	typedef PlayerDataEventReward<&PlayerEventState::save, false> SaveReward;
	typedef PlayerDataEventReward<&PlayerEventState::bump, false> BumpReward;
	typedef PlayerDataEventReward<&PlayerEventState::bumped, true> BumpedPenalty;
	typedef PlayerDataEventReward<&PlayerEventState::demo, false> DemoReward;
	typedef PlayerDataEventReward<&PlayerEventState::demoed, true> DemoedPenalty;

	class GoalReward : public Reward {
	public:
		float scoreReward;
		float concedeScale;

		GoalReward(float scoreReward = 1.0f, float concedeScale = -2.0f)
			: scoreReward(scoreReward), concedeScale(concedeScale) {
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.goalScored)
				return 0;

			bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
			return scored ? scoreReward : concedeScale;
		}
	};

	class KickoffFirstTouchReward : public Reward {
	public:
		float reward_amount = 2.0f;
		float center_tol = 80.0f;
		bool debug = false;

		void Reset(const GameState&) override {
			kickoff_active = false;
			touch_given = false;
		}

		float GetReward(const Player& player, const GameState& state, bool /*isFinal*/) override {
			// Detect kickoff scenario (ball at center)
			if (state.ball.pos.Length2D() <= center_tol) {
				if (!kickoff_active) {
					kickoff_active = true;
					touch_given = false;
					if (debug) std::cout << "[KO] Kickoff started\n";
				}
			}
			else {
				kickoff_active = false;
			}

			if (!kickoff_active || touch_given) return 0.0f;

			// Check if this player touched the ball
			bool touched = player.ballTouchedStep;

			// Fallback: check lastTouchCarID for robustness
			if (!touched && state.lastTouchCarID >= 0) {
				int prev_id = (state.prev ? state.prev->lastTouchCarID : -1);
				touched = ((int)player.carId == state.lastTouchCarID &&
					(int)player.carId != prev_id);
			}

			if (touched) {
				touch_given = true;
				if (debug) {
					std::cout << "[KO] First touch by car " << player.carId
						<< " +" << reward_amount << "\n";
				}
				return reward_amount;
			}

			return 0.0f;
		}

	private:
		bool kickoff_active = false;
		bool touch_given = false;
	};



	class TouchBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.ballTouchedStep;
		}
	};

	class SaveBoostReward : public Reward {
	public:
		float max_reward = 0.3f;       // Leggermente ridotto da 0.5

		float GetReward(const Player& player, const GameState& state, bool) override {
			return max_reward * (player.boost / 100.0f);
		}
	};

	class VelocityPlayerToBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec toBall = (state.ball.pos - player.pos).Normalized();
			Vec velNorm = player.vel.Normalized();

			float reward = (velNorm.Dot(toBall) + 1) / 2;
			return reward;
		}
	};

	class StrongTouchReward : public Reward {
	public:
		float minRewardedVel, maxRewardedVel;

		StrongTouchReward(float minSpeedKPH = 40, float maxSpeedKPH = 130) {
			minRewardedVel = RLGC::Math::KPHToVel(minSpeedKPH);
			maxRewardedVel = RLGC::Math::KPHToVel(maxSpeedKPH);
		}



		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev) return 0;
			if (!player.ballTouchedStep) return 0;

			float hitForce = (state.ball.vel - state.prev->ball.vel).Length();
			if (hitForce < minRewardedVel) return 0;

			return std::min(1.0f, hitForce / maxRewardedVel);
		}
	};



	/*class FlipResetMegaRewardSimple : public Reward {
	public:
		float goal_arm_min_height = 150.0f;
		float min_height_for_gain = 420.0f;

		float gain_base_reward = 6.0f;
		float gain_height_bonus_max = 80.0f;

		uint64_t use_window_ticks = 840;
		float use_base_reward = 10.0f;
		float direction_bonus_max = 10.0f;
		float power_bonus_max = 70.0f;

		float min_power_speed = 350.0f;
		float max_power_speed = 2900.0f;

		float chain_mult_step = 2.0f;
		float chain_mult_cap = 10.0f;

		float goal_arm_min_dir01 = 0.1f;
		uint64_t goal_arm_window_ticks = 480;

		static constexpr float goal_bonus_base = 0.5f;
		static constexpr float goal_bonus_extra_max =1.7f;

		float landing_waste_penalty = -30.0f;

		uint64_t giveaway_window_ticks = 960;
		float giveaway_penalty_mult = 1.0f;

		float penalty_box_depth = 2000.0f;
		float penalty_box_half_width = 1000.0f;

		float follow_after_use_per_tick = 1.0f;
		float follow_after_use_max_dist = 400.0f;
		float follow_after_use_exp = 2.0f;
		uint64_t follow_after_use_window_ticks = 360;

		float touch_after_first_reset_reward = 20.0f;

		float zs_use_zero_sum_scale = 0.0f;

		float module_reward_clamp_min = -8000.0f;
		float module_reward_clamp_max = 16000.0f;

	private:
		struct PlayerState {

			uint64_t last_gain_tick = 0;
			uint64_t gain_cooldown_ticks = 240; // ~2s @120Hz (tickskip-safe perché usi tickCount)

			bool touch20_active = false;
			uint64_t first_reset_tick = 0;

			bool gained = false;
			bool pending_use = false;
			uint64_t flip_used_tick = 0;

			float sequence_score = 0.0f;
			int completed_cycles = 0;

			bool goal_armed = false;
			uint64_t last_use_tick = 0;

			bool giveaway_armed = false;
			uint64_t giveaway_start_tick = 0;
			float seq_snapshot = 0.0f;

			float last_use_reward_this_step = 0.0f;

			bool follow_armed = false;
			uint64_t follow_start_tick = 0;

			uint64_t prev_tick = 0;

			bool aerial_attempt_active = false;
			bool aerial_attempt_resolved = false;
			bool aerial_flip_spent = false;
		};

		std::unordered_map<uint32_t, PlayerState> states;

		inline static float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }

		static Team TeamFromGoalY(float y) { return (y > 0.0f) ? Team::ORANGE : Team::BLUE; }

		const Player* FindOpponent(const Player& player, const GameState& state) const {
			for (const Player& p : state.players)
				if (p.carId != player.carId && p.team != player.team) return &p;
			return nullptr;
		}

		void ResetAll(PlayerState& s) const {
			s.touch20_active = false;
			s.first_reset_tick = 0;

			s.gained = false;
			s.pending_use = false;
			s.flip_used_tick = 0;

			s.sequence_score = 0.0f;
			s.completed_cycles = 0;

			s.goal_armed = false;
			s.last_use_tick = 0;

			s.giveaway_armed = false;
			s.giveaway_start_tick = 0;
			s.seq_snapshot = 0.0f;

			s.last_use_reward_this_step = 0.0f;

			s.follow_armed = false;
			s.follow_start_tick = 0;

			s.aerial_attempt_active = false;
			s.aerial_attempt_resolved = false;
			s.aerial_flip_spent = false;
		}

		void ResetCycleKeepGiveaway(PlayerState& s) const {
			s.touch20_active = false;
			s.first_reset_tick = 0;

			s.gained = false;
			s.pending_use = false;
			s.flip_used_tick = 0;

			s.follow_armed = false;
			s.follow_start_tick = 0;

			s.last_use_reward_this_step = 0.0f;

			s.aerial_attempt_active = false;
			s.aerial_attempt_resolved = false;
			s.aerial_flip_spent = false;
		}

		float ChainMult(const PlayerState& s) const {
			float m = std::pow(chain_mult_step, (float)std::max(0, s.completed_cycles));
			return std::min(m, chain_mult_cap);
		}

		float ComputeTouchAfterFirstReset(PlayerState& s, bool touched_ball_now, uint64_t tick) const {
			if (!s.touch20_active) return 0.0f;
			if (!touched_ball_now) return 0.0f;
			if (tick == s.first_reset_tick) return 0.0f;

			const float mult = ChainMult(s);
			const float r = touch_after_first_reset_reward * mult;
			s.sequence_score += r;
			return r;
		}

		float ComputeGain(PlayerState& s,
			const Player& player,
			const GameState& state,
			bool prev_has_reset,
			bool has_reset_now,
			bool touched_ball_now,
			uint64_t tick)
		{
			if (s.gained) return 0.0f;

			if (!prev_has_reset && has_reset_now && touched_ball_now && player.pos.z >= min_height_for_gain) {
				s.gained = true;

				s.aerial_attempt_active = true;
				s.aerial_attempt_resolved = false;
				s.aerial_flip_spent = false;

				if (!s.touch20_active) {
					s.touch20_active = true;
					s.first_reset_tick = tick;
				}

				const float mult = ChainMult(s);

				float r = gain_base_reward;

				const float ceiling_z = CommonValues::CEILING_Z;
				float t = (player.pos.z - min_height_for_gain) / std::max(1.0f, (ceiling_z - min_height_for_gain));
				t = clamp01(t);

				r += gain_height_bonus_max * t;
				r *= mult;

				s.sequence_score += r;
				return r;
			}
			return 0.0f;
		}

		float ComputeUse(PlayerState& s,
			const Player& player,
			const GameState& state,
			bool touched_ball_now,
			bool prev_has_flip_or_jump,
			bool has_flip_or_jump_now,
			uint64_t tick)
		{
			if (s.gained && !s.pending_use && prev_has_flip_or_jump && !has_flip_or_jump_now) {
				s.pending_use = true;
				s.flip_used_tick = tick;
				s.aerial_flip_spent = true;
			}

			if (!s.pending_use) return 0.0f;

			uint64_t dt = tick - s.flip_used_tick;
			if (dt > use_window_ticks) {
				s.pending_use = false;
				s.gained = false;
				return 0.0f;
			}

			if (!touched_ball_now) return 0.0f;

			Vec opp_goal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;

			Vec to_opp_goal = (opp_goal - state.ball.pos);
			float dist = to_opp_goal.Length();
			Vec to_opp_hat = (dist > 1e-3f) ? (to_opp_goal / dist) : Vec(0, 0, 0);

			float speed = state.ball.vel.Length();
			Vec ball_dir = (speed > 1e-3f) ? (state.ball.vel / speed) : Vec(0, 0, 0);

			float dir01 = clamp01(ball_dir.Dot(to_opp_hat));

			float power01 = (speed - min_power_speed) / std::max(1.0f, (max_power_speed - min_power_speed));
			power01 = clamp01(power01);

			const float mult = ChainMult(s);

			float r = use_base_reward + direction_bonus_max * dir01 + power_bonus_max * power01;
			r *= mult;

			s.sequence_score += r;

			s.goal_armed = (dir01 >= goal_arm_min_dir01) && (player.pos.z >= goal_arm_min_height);
			if (s.goal_armed) s.last_use_tick = tick;

			s.giveaway_armed = true;
			s.giveaway_start_tick = tick;
			s.seq_snapshot = s.sequence_score;

			s.last_use_reward_this_step = r;

			s.follow_armed = true;
			s.follow_start_tick = tick;

			s.completed_cycles++;

			s.pending_use = false;
			s.gained = false;

			s.aerial_attempt_resolved = true;
			s.aerial_attempt_active = false;
			s.aerial_flip_spent = false;

			return r;
		}

		float ComputeFollowAfterUse(PlayerState& s,
			const Player& player,
			const GameState& state,
			uint64_t tick)
		{
			if (!s.follow_armed) return 0.0f;

			uint64_t dtSinceUse = tick - s.follow_start_tick;
			if (dtSinceUse > follow_after_use_window_ticks) {
				s.follow_armed = false;
				return 0.0f;
			}

			if (player.isOnGround) return 0.0f;

			uint64_t stepTicks = 1;
			if (s.prev_tick != 0 && tick > s.prev_tick) stepTicks = tick - s.prev_tick;
			stepTicks = std::min<uint64_t>(stepTicks, 24);

			const Vec d = state.ball.pos - player.pos;
			const float dist3 = d.Length();

			float prox01 = 0.0f;
			if (dist3 < follow_after_use_max_dist) {
				prox01 = 1.0f - (dist3 / std::max(1.0f, follow_after_use_max_dist));
				prox01 = clamp01(prox01);
			}

			const float expo = std::pow(prox01, follow_after_use_exp);
			const float r = follow_after_use_per_tick * expo * (float)stepTicks;

			return r;
		}

		float ComputeGoalBonus(PlayerState& s,
			const Player& player,
			const GameState& state,
			uint64_t tick)
		{
			if (!state.goalScored) return 0.0f;
			if (!s.goal_armed) return 0.0f;

			if (tick - s.last_use_tick > goal_arm_window_ticks) {
				s.goal_armed = false;
				return 0.0f;
			}

			const bool scored = (player.team != TeamFromGoalY(state.ball.pos.y));
			s.goal_armed = false;

			if (!scored) return 0.0f;

			float seq = std::max(0.0f, s.sequence_score);

			const float ceiling_z = CommonValues::CEILING_Z;
			const float ground_z = 0.0f;

			float t = (player.pos.z - ground_z) / std::max(1.0f, (ceiling_z - ground_z));
			t = clamp01(t);

			const float mult = goal_bonus_base + goal_bonus_extra_max * t;
			const float bonus = seq * mult;

			return bonus;
		}

		float ComputeGiveawayPenalty(PlayerState& s,
			const Player& player,
			const GameState& state,
			const Player* opponent,
			uint64_t tick)
		{
			if (!s.giveaway_armed) return 0.0f;
			if (!opponent) return 0.0f;

			const uint64_t dt = tick - s.giveaway_start_tick;
			if (dt > giveaway_window_ticks) {
				s.giveaway_armed = false;
				return 0.0f;
			}

			const int lastTouchId = state.lastTouchCarID;
			const bool opp_touched = (lastTouchId >= 0 && (uint32_t)lastTouchId == opponent->carId);
			if (!opp_touched) return 0.0f;

			const bool meInAir = !player.isOnGround;
			const bool oppInAir = !opponent->isOnGround;

			const float ballGroundEps = 5.0f;
			const bool ballInAir = (state.ball.pos.z > (CommonValues::BALL_RADIUS + ballGroundEps));

			if (meInAir && oppInAir && ballInAir) {
				s.giveaway_armed = false;
				return 0.0f;
			}

			auto IsInPenaltyBoxApprox = [&](const Player& opp) -> bool {
				const float sgn = (opp.team == Team::ORANGE) ? 1.0f : -1.0f;
				const float goalY = (opp.team == Team::ORANGE) ? CommonValues::ORANGE_GOAL_CENTER.y : CommonValues::BLUE_GOAL_CENTER.y;

				const float depthFromGoal = (goalY - opp.pos.y) * sgn;

				const bool inDepth = (depthFromGoal >= 0.0f && depthFromGoal <= penalty_box_depth);
				const bool inWidth = (std::abs(opp.pos.x) <= penalty_box_half_width);
				return inDepth && inWidth;
				};

			if (IsInPenaltyBoxApprox(*opponent)) {
				s.giveaway_armed = false;
				return 0.0f;
			}

			s.giveaway_armed = false;

			const float snap = std::max(0.0f, s.seq_snapshot);
			const float pen = -giveaway_penalty_mult * snap;

			ResetAll(s);
			return pen;
		}

	public:
		void Reset(const GameState&) override { states.clear(); }

		float GetReward(const Player& player, const GameState& state, bool) override {
			auto& s = states[player.carId];

			const uint64_t tick = state.lastArena ? state.lastArena->tickCount : state.lastTickCount;

			if (s.prev_tick == 0) s.prev_tick = tick;
			uint64_t stepTicks = 1;
			if (tick > s.prev_tick) stepTicks = tick - s.prev_tick;
			stepTicks = std::min<uint64_t>(stepTicks, 24);
			s.prev_tick = tick;

			(void)stepTicks;

			s.last_use_reward_this_step = 0.0f;

			const bool on_ground = player.isOnGround;

			const bool has_reset_now = player.HasFlipReset();
			const bool has_flip_or_jump_now = player.HasFlipOrJump();
			const bool touched_ball_now = player.ballTouchedStep;

			const bool prev_has_reset = (player.prev != nullptr) ? player.prev->HasFlipReset() : false;
			const bool prev_has_flip_or_jump = (player.prev != nullptr) ? player.prev->HasFlipOrJump() : true;

			const Player* opponent = FindOpponent(player, state);

			float core = 0.0f;
			float goal_part = 0.0f;

			goal_part += ComputeGoalBonus(s, player, state, tick);
			if (goal_part > 0.0f) {
				ResetAll(s);
				core = std::clamp(core, module_reward_clamp_min, module_reward_clamp_max);
				return core + goal_part;
			}

			core += ComputeGiveawayPenalty(s, player, state, opponent, tick);
			core += ComputeFollowAfterUse(s, player, state, tick);

			if (on_ground) {
				const bool wasted = (s.aerial_attempt_active && !s.aerial_attempt_resolved) || s.gained || s.pending_use || s.aerial_flip_spent;
				if (wasted) core += landing_waste_penalty;

				s.completed_cycles = 0;
				s.sequence_score = 0.0f;

				ResetCycleKeepGiveaway(s);

				core = std::clamp(core, module_reward_clamp_min, module_reward_clamp_max);
				return core + goal_part;
			}

			const float g = ComputeGain(s, player, state, prev_has_reset, has_reset_now, touched_ball_now, tick);
			core += g;

			core += ComputeTouchAfterFirstReset(s, touched_ball_now, tick);

			if (g > 0.0f) {
				s.giveaway_armed = true;
				s.giveaway_start_tick = tick;
				s.seq_snapshot = s.sequence_score;
			}

			core += ComputeUse(
				s, player, state, touched_ball_now,
				prev_has_flip_or_jump, has_flip_or_jump_now, tick
			);

			core = std::clamp(core, module_reward_clamp_min, module_reward_clamp_max);
			return core + goal_part;
		}

		virtual std::vector<float> GetAllRewards(const GameState& state, bool final) override {
			const int n = (int)state.players.size();
			std::vector<float> rewards(n, 0.0f);
			if (n == 0) return rewards;

			for (int i = 0; i < n; ++i)
				rewards[i] = GetReward(state.players[i], state, final);

			if (zs_use_zero_sum_scale <= 0.0f) return rewards;

			float teamUseSum[2] = { 0.0f, 0.0f };
			int teamCounts[2] = { 0, 0 };

			for (int i = 0; i < n; ++i) {
				const Player& p = state.players[i];
				const int teamIdx = (int)p.team;

				auto it = states.find(p.carId);
				float use = 0.0f;
				if (it != states.end()) use = it->second.last_use_reward_this_step;

				teamUseSum[teamIdx] += use;
				teamCounts[teamIdx] += 1;
			}

			float avgUseTeam[2] = { 0.0f, 0.0f };
			for (int t = 0; t < 2; ++t)
				if (teamCounts[t] > 0) avgUseTeam[t] = teamUseSum[t] / (float)teamCounts[t];

			for (int i = 0; i < n; ++i) {
				const Player& p = state.players[i];
				const int teamIdx = (int)p.team;
				const int oppIdx = 1 - teamIdx;
				rewards[i] -= avgUseTeam[oppIdx] * zs_use_zero_sum_scale;
			}

			return rewards;
		}
	};
	*/



class FlipResetMegaRewardSimple : public Reward {
public:
	// =========================
	// PARAMETRI (tuning)
	// =========================
	float goal_arm_min_height = 150.0f;
	float min_height_for_gain = 420.0f;

	float gain_base_reward = 6.0f;
	float gain_height_bonus_max = 60.0f; // un filo meno spiky

	uint64_t use_window_ticks = 840;
	float use_base_reward = 10.0f;
	float direction_bonus_max = 8.0f;
	float power_bonus_max = 60.0f;

	float min_power_speed = 350.0f;
	float max_power_speed = 2900.0f;

	// più morbido: la chain 2.0 esplode ancora presto (2,4,8...)
	float chain_mult_step = 1.8f;
	float chain_mult_cap = 6.0f;

	float goal_arm_min_dir01 = 0.15f;
	uint64_t goal_arm_window_ticks = 540;

	static constexpr float goal_bonus_base = 0.45f;
	static constexpr float goal_bonus_extra_max = 1.25f;

	float landing_waste_penalty = -25.0f;

	uint64_t giveaway_window_ticks = 900;
	float giveaway_penalty_mult = 0.75f;

	float penalty_box_depth = 2200.0f;
	float penalty_box_half_width = 1100.0f;

	float follow_after_use_per_tick = 0.7f;
	float follow_after_use_max_dist = 450.0f;
	float follow_after_use_exp = 2.0f;
	uint64_t follow_after_use_window_ticks = 420;

	float touch_after_first_reset_reward = 12.0f;

	float zs_use_zero_sum_scale = 0.0f;

	float module_reward_clamp_min = -4000.0f;
	float module_reward_clamp_max = 8000.0f;

	// =========================
	// NUOVI PARAMETRI ROBUSTEZZA
	// =========================
	uint64_t gain_touch_window_ticks = 24;   // accetta reset/touch entro 6 tick
	uint64_t gain_reset_arm_ticks = 16;     // quanto dura l'arm prima di scadere

private:
	struct PlayerState {
		// cooldown reale
		uint64_t last_gain_tick = 0;
		uint64_t gain_cooldown_ticks = 240;

		// robust gain arm
		bool gain_armed = false;
		uint64_t gain_arm_tick = 0;

		bool touch20_active = false;
		uint64_t first_reset_tick = 0;

		bool gained = false;
		bool pending_use = false;
		uint64_t flip_used_tick = 0;

		float sequence_score = 0.0f;
		int completed_cycles = 0;

		bool goal_armed = false;
		uint64_t last_use_tick = 0;

		bool giveaway_armed = false;
		uint64_t giveaway_start_tick = 0;
		float seq_snapshot = 0.0f;

		float last_use_reward_this_step = 0.0f;

		bool follow_armed = false;
		uint64_t follow_start_tick = 0;

		uint64_t prev_tick = 0;

		// landing waste: solo se hai iniziato un tentativo
		bool attempt_started = false;
		bool attempt_resolved = false;
		bool flip_spent_in_attempt = false;
	};

	std::unordered_map<uint32_t, PlayerState> states;

	inline static float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }
	static Team TeamFromGoalY(float y) { return (y > 0.0f) ? Team::ORANGE : Team::BLUE; }

	const Player* FindOpponent(const Player& player, const GameState& state) const {
		for (const Player& p : state.players)
			if (p.carId != player.carId && p.team != player.team) return &p;
		return nullptr;
	}

	// Touch robusto: ballTouchedStep + fallback lastTouchCarID delta
	bool TouchedBallNowRobust(const Player& player, const GameState& state) const {
		if (player.ballTouchedStep) return true;
		if (state.lastTouchCarID < 0) return false;
		int prev_id = (state.prev ? state.prev->lastTouchCarID : -1);
		return ((int)player.carId == state.lastTouchCarID && (int)player.carId != prev_id);
	}

	void ResetAll(PlayerState& s) const {
		s.gain_armed = false;
		s.gain_arm_tick = 0;

		s.touch20_active = false;
		s.first_reset_tick = 0;

		s.gained = false;
		s.pending_use = false;
		s.flip_used_tick = 0;

		s.sequence_score = 0.0f;
		s.completed_cycles = 0;

		s.goal_armed = false;
		s.last_use_tick = 0;

		s.giveaway_armed = false;
		s.giveaway_start_tick = 0;
		s.seq_snapshot = 0.0f;

		s.last_use_reward_this_step = 0.0f;

		s.follow_armed = false;
		s.follow_start_tick = 0;

		s.attempt_started = false;
		s.attempt_resolved = false;
		s.flip_spent_in_attempt = false;
	}

	// reset ciclo ma non azzera totalmente "memoria" di giveaway
	void ResetCycleKeepGiveaway(PlayerState& s) const {
		s.gain_armed = false;
		s.gain_arm_tick = 0;

		s.touch20_active = false;
		s.first_reset_tick = 0;

		s.gained = false;
		s.pending_use = false;
		s.flip_used_tick = 0;

		s.follow_armed = false;
		s.follow_start_tick = 0;

		s.last_use_reward_this_step = 0.0f;

		s.attempt_started = false;
		s.attempt_resolved = false;
		s.flip_spent_in_attempt = false;
	}

	float ChainMult(const PlayerState& s) const {
		float m = std::pow(chain_mult_step, (float)std::max(0, s.completed_cycles));
		return std::min(m, chain_mult_cap);
	}

	float ComputeTouchAfterFirstReset(PlayerState& s, bool touched_ball_now, uint64_t tick) const {
		if (!s.touch20_active) return 0.0f;
		if (!touched_ball_now) return 0.0f;
		if (tick == s.first_reset_tick) return 0.0f;

		const float mult = ChainMult(s);
		const float r = touch_after_first_reset_reward * mult;
		s.sequence_score += r;
		return r;
	}

	// ===== GAIN: robust + cooldown + window =====
	float ComputeGain(PlayerState& s,
		const Player& player,
		const GameState& state,
		bool prev_has_reset,
		bool has_reset_now,
		bool touched_ball_now,
		uint64_t tick)
	{
		// cooldown
		if (s.last_gain_tick != 0 && tick - s.last_gain_tick < s.gain_cooldown_ticks) {
			// puoi comunque armare, ma non pagare gain
		}

		// arma quando compare un reset
		if (!prev_has_reset && has_reset_now) {
			s.gain_armed = true;
			s.gain_arm_tick = tick;
		}

		// scadenza arm
		if (s.gain_armed && (tick - s.gain_arm_tick > gain_reset_arm_ticks)) {
			s.gain_armed = false;
		}

		if (s.gained) return 0.0f;

		// condizione: armato + touch entro finestra + altezza
		if (s.gain_armed && touched_ball_now && player.pos.z >= min_height_for_gain) {

			// window rispetto a quando hai armato
			if (tick - s.gain_arm_tick <= gain_touch_window_ticks) {

				// applica cooldown: se in cooldown, non pagare (ma lascia arm per il prossimo?)
				if (s.last_gain_tick != 0 && tick - s.last_gain_tick < s.gain_cooldown_ticks) {
					// non pagare, ma disarma per evitare spam
					s.gain_armed = false;
					return 0.0f;
				}

				s.gain_armed = false;
				s.gained = true;
				s.last_gain_tick = tick;

				s.attempt_started = true;
				s.attempt_resolved = false;
				s.flip_spent_in_attempt = false;

				if (!s.touch20_active) {
					s.touch20_active = true;
					s.first_reset_tick = tick;
				}

				const float mult = ChainMult(s);

				float r = gain_base_reward;

				const float ceiling_z = CommonValues::CEILING_Z;
				float t = (player.pos.z - min_height_for_gain) / std::max(1.0f, (ceiling_z - min_height_for_gain));
				t = clamp01(t);

				r += gain_height_bonus_max * t;
				r *= mult;

				s.sequence_score += r;

				// arma giveaway snapshot già qui
				s.giveaway_armed = true;
				s.giveaway_start_tick = tick;
				s.seq_snapshot = s.sequence_score;

				return r;
			}
		}

		return 0.0f;
	}

	float ComputeUse(PlayerState& s,
		const Player& player,
		const GameState& state,
		bool touched_ball_now,
		bool prev_has_flip_or_jump,
		bool has_flip_or_jump_now,
		uint64_t tick)
	{
		// spesa flip dopo gain
		if (s.gained && !s.pending_use && prev_has_flip_or_jump && !has_flip_or_jump_now) {
			s.pending_use = true;
			s.flip_used_tick = tick;
			s.flip_spent_in_attempt = true;
		}

		if (!s.pending_use) return 0.0f;

		uint64_t dt = tick - s.flip_used_tick;
		if (dt > use_window_ticks) {
			s.pending_use = false;
			s.gained = false;
			return 0.0f;
		}

		if (!touched_ball_now) return 0.0f;

		Vec opp_goal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;

		Vec to_opp_goal = (opp_goal - state.ball.pos);
		float dist = to_opp_goal.Length();
		Vec to_opp_hat = (dist > 1e-3f) ? (to_opp_goal / dist) : Vec(0, 0, 0);

		float speed = state.ball.vel.Length();
		Vec ball_dir = (speed > 1e-3f) ? (state.ball.vel / speed) : Vec(0, 0, 0);

		float dir01 = clamp01(ball_dir.Dot(to_opp_hat));

		float power01 = (speed - min_power_speed) / std::max(1.0f, (max_power_speed - min_power_speed));
		power01 = clamp01(power01);

		const float mult = ChainMult(s);

		float r = use_base_reward + direction_bonus_max * dir01 + power_bonus_max * power01;
		r *= mult;

		s.sequence_score += r;

		// arma goal bonus solo se davvero sta andando verso goal e sei almeno un po' alto
		s.goal_armed = (dir01 >= goal_arm_min_dir01) && (player.pos.z >= goal_arm_min_height);
		if (s.goal_armed) s.last_use_tick = tick;

		// aggiorna giveaway snapshot al momento dell'use
		s.giveaway_armed = true;
		s.giveaway_start_tick = tick;
		s.seq_snapshot = s.sequence_score;

		s.last_use_reward_this_step = r;

		s.follow_armed = true;
		s.follow_start_tick = tick;

		// chiudi ciclo
		s.completed_cycles++;
		s.pending_use = false;
		s.gained = false;

		s.attempt_resolved = true;
		s.attempt_started = false;
		s.flip_spent_in_attempt = false;

		return r;
	}

	float ComputeFollowAfterUse(PlayerState& s,
		const Player& player,
		const GameState& state,
		uint64_t tick)
	{
		if (!s.follow_armed) return 0.0f;

		uint64_t dtSinceUse = tick - s.follow_start_tick;
		if (dtSinceUse > follow_after_use_window_ticks) {
			s.follow_armed = false;
			return 0.0f;
		}

		if (player.isOnGround) return 0.0f;

		uint64_t stepTicks = 1;
		if (s.prev_tick != 0 && tick > s.prev_tick) stepTicks = tick - s.prev_tick;
		stepTicks = std::min<uint64_t>(stepTicks, 24);

		const Vec d = state.ball.pos - player.pos;
		const float dist3 = d.Length();

		float prox01 = 0.0f;
		if (dist3 < follow_after_use_max_dist) {
			prox01 = 1.0f - (dist3 / std::max(1.0f, follow_after_use_max_dist));
			prox01 = clamp01(prox01);
		}

		const float expo = std::pow(prox01, follow_after_use_exp);
		const float r = follow_after_use_per_tick * expo * (float)stepTicks;

		// opzionale: accumula un po' nella sequence_score così il goal bonus “sente” la chase
		s.sequence_score += 0.25f * r;

		return r;
	}

	float ComputeGoalBonus(PlayerState& s,
		const Player& player,
		const GameState& state,
		uint64_t tick)
	{
		if (!state.goalScored) return 0.0f;
		if (!s.goal_armed) return 0.0f;

		if (tick - s.last_use_tick > goal_arm_window_ticks) {
			s.goal_armed = false;
			return 0.0f;
		}

		const bool scored = (player.team != TeamFromGoalY(state.ball.pos.y));
		s.goal_armed = false;

		if (!scored) return 0.0f;

		float seq = std::max(0.0f, s.sequence_score);

		const float ceiling_z = CommonValues::CEILING_Z;
		const float ground_z = 0.0f;

		float t = (player.pos.z - ground_z) / std::max(1.0f, (ceiling_z - ground_z));
		t = clamp01(t);

		const float mult = goal_bonus_base + goal_bonus_extra_max * t;
		return seq * mult;
	}

	float ComputeGiveawayPenalty(PlayerState& s,
		const Player& player,
		const GameState& state,
		const Player* opponent,
		uint64_t tick)
	{
		if (!s.giveaway_armed) return 0.0f;
		if (!opponent) return 0.0f;

		const uint64_t dt = tick - s.giveaway_start_tick;
		if (dt > giveaway_window_ticks) {
			s.giveaway_armed = false;
			return 0.0f;
		}

		const int lastTouchId = state.lastTouchCarID;
		const bool opp_touched = (lastTouchId >= 0 && (uint32_t)lastTouchId == opponent->carId);
		if (!opp_touched) return 0.0f;

		// non penalizzare se è chiaramente “aerial contest”
		const bool meInAir = !player.isOnGround;
		const bool oppInAir = !opponent->isOnGround;
		const float ballGroundEps = 5.0f;
		const bool ballInAir = (state.ball.pos.z > (CommonValues::BALL_RADIUS + ballGroundEps));
		if (meInAir && oppInAir && ballInAir) {
			s.giveaway_armed = false;
			return 0.0f;
		}

		auto IsInPenaltyBoxApprox = [&](const Player& opp) -> bool {
			const float sgn = (opp.team == Team::ORANGE) ? 1.0f : -1.0f;
			const float goalY = (opp.team == Team::ORANGE) ? CommonValues::ORANGE_GOAL_CENTER.y : CommonValues::BLUE_GOAL_CENTER.y;
			const float depthFromGoal = (goalY - opp.pos.y) * sgn;
			const bool inDepth = (depthFromGoal >= 0.0f && depthFromGoal <= penalty_box_depth);
			const bool inWidth = (std::abs(opp.pos.x) <= penalty_box_half_width);
			return inDepth && inWidth;
			};

		// se l’avversario la tocca nella sua box, non è “giveaway” (è difesa)
		if (IsInPenaltyBoxApprox(*opponent)) {
			s.giveaway_armed = false;
			return 0.0f;
		}

		s.giveaway_armed = false;

		const float snap = std::max(0.0f, s.seq_snapshot);
		const float pen = -giveaway_penalty_mult * snap;

		ResetAll(s);
		return pen;
	}

public:
	void Reset(const GameState&) override { states.clear(); }

	float GetReward(const Player& player, const GameState& state, bool) override {
		auto& s = states[player.carId];

		const uint64_t tick = state.lastArena ? state.lastArena->tickCount : state.lastTickCount;

		if (s.prev_tick == 0) s.prev_tick = tick;
		uint64_t stepTicks = (tick > s.prev_tick) ? (tick - s.prev_tick) : 1;
		stepTicks = std::min<uint64_t>(stepTicks, 24);
		s.prev_tick = tick;

		s.last_use_reward_this_step = 0.0f;

		const bool on_ground = player.isOnGround;

		const bool has_reset_now = player.HasFlipReset();
		const bool has_flip_or_jump_now = player.HasFlipOrJump();

		const bool prev_has_reset = (player.prev != nullptr) ? player.prev->HasFlipReset() : false;
		const bool prev_has_flip_or_jump = (player.prev != nullptr) ? player.prev->HasFlipOrJump() : true;

		// touch robusto
		const bool touched_ball_now = TouchedBallNowRobust(player, state);

		const Player* opponent = FindOpponent(player, state);

		float core = 0.0f;

		// 1) goal bonus prima
		const float goal_part = ComputeGoalBonus(s, player, state, tick);
		if (goal_part > 0.0f) {
			ResetAll(s);
			return std::clamp(core, module_reward_clamp_min, module_reward_clamp_max) + goal_part;
		}

		// 2) giveaway + follow
		core += ComputeGiveawayPenalty(s, player, state, opponent, tick);
		core += ComputeFollowAfterUse(s, player, state, tick);

		// 3) on ground: penalizza SOLO se avevi iniziato un tentativo e non l'hai risolto
		if (on_ground) {
			const bool wasted = (s.attempt_started && !s.attempt_resolved) || s.gained || s.pending_use || s.flip_spent_in_attempt;
			if (wasted) core += landing_waste_penalty;

			// reset “soft”
			s.completed_cycles = 0;
			s.sequence_score = 0.0f;
			ResetCycleKeepGiveaway(s);

			return std::clamp(core, module_reward_clamp_min, module_reward_clamp_max);
		}

		// 4) gain (robusto)
		core += ComputeGain(s, player, state, prev_has_reset, has_reset_now, touched_ball_now, tick);

		// 5) touch dopo primo reset
		core += ComputeTouchAfterFirstReset(s, touched_ball_now, tick);

		// 6) use (spesa flip + touch)
		core += ComputeUse(s, player, state, touched_ball_now, prev_has_flip_or_jump, has_flip_or_jump_now, tick);

		return std::clamp(core, module_reward_clamp_min, module_reward_clamp_max);
	}

	std::vector<float> GetAllRewards(const GameState& state, bool final) override {
		const int n = (int)state.players.size();
		std::vector<float> rewards(n, 0.0f);
		if (n == 0) return rewards;

		for (int i = 0; i < n; ++i)
			rewards[i] = GetReward(state.players[i], state, final);

		if (zs_use_zero_sum_scale <= 0.0f) return rewards;

		float teamUseSum[2] = { 0.0f, 0.0f };
		int teamCounts[2] = { 0, 0 };

		for (int i = 0; i < n; ++i) {
			const Player& p = state.players[i];
			const int teamIdx = (int)p.team;

			auto it = states.find(p.carId);
			float use = 0.0f;
			if (it != states.end()) use = it->second.last_use_reward_this_step;

			teamUseSum[teamIdx] += use;
			teamCounts[teamIdx] += 1;
		}

		float avgUseTeam[2] = { 0.0f, 0.0f };
		for (int t = 0; t < 2; ++t)
			if (teamCounts[t] > 0) avgUseTeam[t] = teamUseSum[t] / (float)teamCounts[t];

		for (int i = 0; i < n; ++i) {
			const Player& p = state.players[i];
			const int teamIdx = (int)p.team;
			const int oppIdx = 1 - teamIdx;
			rewards[i] -= avgUseTeam[oppIdx] * zs_use_zero_sum_scale;
		}

		return rewards;
	}
};

	class HyperNoStackReward : public Reward {
	private:
		struct PlayerState {
			bool wasOnWall = false;
			bool dashUsed = false;
			bool hadFlipBefore = false;
			bool wasFlipping = false;
			Vec prevVel = Vec(0, 0, 0);
			// int lastDemoCount = 0; // Removed: Not supported by current Player struct
		};

		std::map<uint32_t, PlayerState> playerStates;

		// --- Configuration ---
		float wallHeightThreshold;

		// Base Rewards
		float dashRewardBase;
		float resetRewardBase;
		float wavedashRewardBase;
		float zapDashBaseReward;
		// float demoReward; // Removed temporarily to fix build

		// Quality Scaling
		float accelerationScalar;

		// Penalties
		float wallStayPenalty;
		float supersonicBoostPenalty;

		// Zap Dash Config
		float zapMinSpeedGain;
		float zapMinNoseDown;
		float zapMinFwdDot;

		bool debug;

	public:
		HyperNoStackReward(
			// Base Values
			float dashRewardBase = 12.0f,
			float resetRewardBase = 16.0f,
			float wavedashRewardBase = 10.0f,
			float zapDashBaseReward = 20.0f,
			// float demoReward = 30.0f, 
			// Scaling
			float accelerationScalar = 1.0f,
			// Penalties
			float wallStayPenalty = -0.1f,
			float supersonicBoostPenalty = -0.1f,
			// Debug
			bool debug = false
		) :
			wallHeightThreshold(100.0f),
			dashRewardBase(dashRewardBase), resetRewardBase(resetRewardBase),
			wavedashRewardBase(wavedashRewardBase), zapDashBaseReward(zapDashBaseReward),
			// demoReward(demoReward),
			accelerationScalar(accelerationScalar),
			wallStayPenalty(wallStayPenalty), supersonicBoostPenalty(supersonicBoostPenalty),
			zapMinSpeedGain(500.0f), zapMinNoseDown(0.5f), zapMinFwdDot(0.7f),
			debug(debug)
		{
		}

		virtual void Reset(const GameState& initialState) override {
			playerStates.clear();
			for (const auto& player : initialState.players) {
				InitState(player);
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev) return 0.0f;

			if (playerStates.find(player.carId) == playerStates.end()) {
				InitState(player);
			}
			PlayerState& st = playerStates[player.carId];

			float totalReward = 0.0f;
			float dt = 1.0f / 120.0f;

			// --- PRE-CALCULATIONS ---
			bool isOnWall = IsOnWall(player.pos);
			bool hasFlip = player.HasFlipOrJump();

			// Calculate Acceleration Quality
			float acceleration = (player.vel - st.prevVel).Length() / dt;
			float normAccel = std::min(acceleration / 5000.0f, 2.0f);
			float qualityMult = 1.0f + (normAccel * accelerationScalar);

			// =========================================================
			// MECHANIC DETECTION
			// =========================================================

			// 1. DEMOLITION (Disabled to fix build error)
			/*
			if (player.match_demolitions > st.lastDemoCount) {
				totalReward += demoReward * qualityMult;
				if (debug) printf("ID:%d | DEMO | Reward: %.2f\n", player.carId, demoReward * qualityMult);
			}
			*/

			// 2. ZAP DASH
			float zapRatio = CheckZapDash(player);
			if (zapRatio > 0.0f) {
				float r = zapDashBaseReward * zapRatio * qualityMult;
				totalReward += r;
				if (debug) printf("ID:%d | ZapDash | Reward: %.2f\n", player.carId, r);
			}

			// 3. WALL DASH (LAUNCH)
			if (st.wasOnWall && st.hadFlipBefore && !hasFlip && !isOnWall) {
				st.dashUsed = true;
				float r = dashRewardBase * qualityMult;
				totalReward += r;
				if (debug) printf("ID:%d | WallDash_Launch | Reward: %.2f\n", player.carId, r);
			}

			// 4. WALL DASH (RESET)
			if (st.dashUsed && isOnWall && hasFlip) {
				st.dashUsed = false; // Cycle complete
				float r = resetRewardBase * qualityMult;
				totalReward += r;
				if (debug) printf("ID:%d | WallDash_Reset | Reward: %.2f\n", player.carId, r);
			}

			// 5. WAVEDASH
			const Player* prevPlayer = nullptr;
			for (const auto& p : state.prev->players) { if (p.carId == player.carId) { prevPlayer = &p; break; } }

			bool justLanded = player.isOnGround && prevPlayer && !prevPlayer->isOnGround;
			if (justLanded && (player.isFlipping || st.wasFlipping)) {
				float r = wavedashRewardBase * qualityMult;
				totalReward += r;
				if (debug) printf("ID:%d | Wavedash | Reward: %.2f\n", player.carId, r);
			}

			// =========================================================
			// PENALTIES
			// =========================================================

			// 1. Supersonic Boost Waste
			if (prevPlayer && player.isSupersonic && player.boost < prevPlayer->boost) {
				totalReward += supersonicBoostPenalty;
			}

			// 2. Conditional Wall Stay Penalty
			if (isOnWall && st.dashUsed && player.vel.Length() < 600.0f) {
				totalReward += wallStayPenalty;
			}

			// =========================================================
			// STATE UPDATES
			// =========================================================
			st.wasOnWall = isOnWall;
			st.hadFlipBefore = hasFlip;
			st.wasFlipping = player.isFlipping;
			st.prevVel = player.vel;
			// st.lastDemoCount = player.match_demolitions; // Disabled

			if (player.isOnGround) {
				st.dashUsed = false;
			}

			return totalReward;
		}

	private:
		void InitState(const Player& p) {
			PlayerState newState;
			newState.prevVel = p.vel;
			newState.hadFlipBefore = p.HasFlipOrJump();
			// newState.lastDemoCount = p.match_demolitions; // Disabled
			playerStates[p.carId] = newState;
		}

		float CheckZapDash(const Player& current) {
			if (!current.prev || !current.prev->prev || !current.prev->prev->prev) return 0.0f;

			const Player* p0 = &current;
			const Player* p1 = current.prev;
			const Player* p2 = p1->prev;
			const Player* p3 = p2->prev;

			bool wasAirborneT3 = !p3->isOnGround;
			bool landedT2 = wasAirborneT3 && p2->isOnGround;
			bool noseDown = p2->rotMat.forward.z < -zapMinNoseDown;
			bool wasMovingDown = p2->vel.z < -200.0f;
			bool executingFlip = p0->isFlipping;

			if (landedT2 && noseDown && wasMovingDown && executingFlip) {
				Vec carFwd = p1->rotMat.forward;
				Vec velDir = p0->vel.Normalized();
				if (carFwd.Dot(velDir) > zapMinFwdDot) {
					float gain = p0->vel.Length() - p2->vel.Length();
					if (gain > zapMinSpeedGain) {
						float bonus = (p0->vel.Length() > 2200.f) ? 1.5f : 1.0f;
						return (gain / 1000.0f) * bonus;
					}
				}
			}
			return 0.0f;
		}

		bool IsOnWall(const Vec& pos) {
			if (pos.z < wallHeightThreshold) return false;
			if (std::abs(std::abs(pos.x) - 4096.0f) < 150.0f) return true;
			if (std::abs(std::abs(pos.y) - 5120.0f) < 150.0f) return true;
			return false;
		}
	};

	/*class FlyToGoal : public Reward {
	public:
		// =========================
		// PARAMETRI (tunable)
		// =========================
		float min_air_height = 260.0f;

		// contesto "airdribble bouncy": non serve palla incollata
		float ctx_max_ball_dist = 1200.0f;
		float ctx_min_speed_2d = 450.0f;

		// base shaping (per-second, poi *dt)
		float base_fly_per_sec = 1.5f;
		float toward_ball_weight = 0.55f;
		float toward_goal_weight = 0.85f;   // auto->porta avversaria (2D)
		float proximity_weight = 0.35f;

		// ===== SCALING GLOBALE verso porta avversaria (auto -> goal opp) =====
		// lascia un minimo >0 anche con allineamento 0,
		// perché a volte conviene volare "strano" anche in difesa/clear.
		float goal_scale_min = 0.25f;   // minimo moltiplicatore quando alignment=0
		float goal_scale_pow = 2.0f;    // più alto => più severo quando non sei allineato

		// airroll detection
		float min_roll_rate = 3.0f;      // rad/s circa
		float target_roll_rate = 6.0f;
		float min_roll_purity = 0.70f;   // roll / |angVel|
		int   streak_needed = 3;         // skip-friendly
		int   streak_cap = 25;

		// airroll reward/punish (per-second, poi *dt)
		float airroll_per_sec = 6.0f;    // deve "spaccare"
		float no_roll_pen_per_sec = 0.2f; // lieve ma costante: se sei nel contesto e NON rolli, perdi

		// extra: sotto-palla (per airdribble bouncy)
		float under_ball_min = 20.0f;
		float under_ball_max = 520.0f;
		float under_ball_bonus_per_sec = 0.3f;

		// ===== BONUS TOCCO AEREO =====
		float air_touch_bonus = 4.0f;

		bool debug = false;

	private:
		struct St {
			uint64_t prevTick = 0;
			float rollEma = 0.0f;
			int rollStreak = 0;
			int lastRollSign = 0;
		};
		std::unordered_map<uint32_t, St> st_;

		inline static float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }

		uint64_t GetTick(const GameState& state) const {
			return state.lastArena ? state.lastArena->tickCount : state.lastTickCount;
		}

	public:
		std::string GetName() override { return "FlyToGoal"; }

		void Reset(const GameState&) override { st_.clear(); }

		float GetReward(const Player& player, const GameState& state, bool) override {
			auto& s = st_[player.carId];

			// ===== dt tickskip-safe =====
			const uint64_t tick = GetTick(state);
			float dt = 1.0f / 120.0f;
			if (s.prevTick != 0 && tick > s.prevTick) {
				dt = (float)(tick - s.prevTick) / 120.0f;
				dt = std::max(1.0f / 120.0f, std::min(dt, 0.2f));
			}
			s.prevTick = tick;

			// ======================================================
			// TOUCH DETECTION (come nelle altre reward):
			// 1) player.ballTouchedStep
			// 2) fallback: state.lastTouchCarID cambiato rispetto a prev
			// ======================================================
			bool touched = player.ballTouchedStep;

			if (!touched && state.lastTouchCarID >= 0) {
				int prev_id = (state.prev ? state.prev->lastTouchCarID : -1);
				touched = ((int)player.carId == state.lastTouchCarID &&
					(int)player.carId != prev_id);
			}

			float reward = 0.0f;

			// ===== BONUS: tocco in aria (non scalato da goalMult) =====
			// così vale anche quando vai "verso casa" per clear/difesa.
			if (touched && !player.isOnGround) {
				reward += air_touch_bonus;
			}

			// ===== gating minimo per lo shaping di volo =====
			if (player.isOnGround || player.pos.z < min_air_height)
				return reward;

			// porta avversaria
			Vec oppGoal = (player.team == Team::BLUE)
				? CommonValues::ORANGE_GOAL_CENTER
				: CommonValues::BLUE_GOAL_CENTER;

			// contesto: palla non troppo lontana
			Vec toBall = state.ball.pos - player.pos;
			float dist3 = toBall.Length();
			if (dist3 > ctx_max_ball_dist)
				return reward;

			// velocità 2D minima
			Vec v2(player.vel.x, player.vel.y, 0.0f);
			float v2len = v2.Length();
			if (v2len < ctx_min_speed_2d)
				return reward;

			Vec v2hat = (v2len > 1e-6f) ? (v2 / v2len) : Vec(0, 0, 0);

			// direzione auto->palla 2D
			Vec tb2(toBall.x, toBall.y, 0.0f);
			float tbL = tb2.Length();
			Vec tbHat = (tbL > 1e-6f) ? (tb2 / tbL) : Vec(0, 0, 0);

			// direzione auto->porta avversaria 2D
			Vec cg2(oppGoal.x - player.pos.x, oppGoal.y - player.pos.y, 0.0f);
			float cgL = cg2.Length();
			Vec cgHat = (cgL > 1e-6f) ? (cg2 / cgL) : Vec(0, 0, 0);

			// proximity (bouncy: ok anche non attaccata)
			float prox01 = clamp01(1.0f - (dist3 / ctx_max_ball_dist));

			// spinta verso palla e verso porta (auto->goal)
			float towardBall = clamp01(v2hat.Dot(tbHat));
			float towardOppGoal = clamp01(v2hat.Dot(cgHat)); // 0 se vai di lato o verso la tua porta

			float baseShape =
				toward_ball_weight * towardBall +
				toward_goal_weight * towardOppGoal +
				proximity_weight * prox01;

			baseShape = clamp01(baseShape);

			// base fly
			float flyReward = base_fly_per_sec * baseShape * dt;

			// ===== sotto-palla (airdribble bouncy) =====
			float gap = state.ball.pos.z - player.pos.z; // palla sopra l'auto
			if (gap >= under_ball_min && gap <= under_ball_max) {
				float g01 = clamp01((gap - under_ball_min) / std::max(1.0f, (under_ball_max - under_ball_min)));
				flyReward += under_ball_bonus_per_sec * (0.4f + 0.6f * g01) * dt;
			}

			// ===== AIRROLL: "deve rollare" =====
			Vec forward = player.rotMat.forward;
			float angMag = player.angVel.Length();
			float rollSigned = player.angVel.Dot(forward);
			float rollAbs = std::fabs(rollSigned);
			float purity = rollAbs / std::max(1e-6f, angMag);

			// EMA sul rollAbs (stabile con skip)
			float alpha = 0.30f;
			s.rollEma += (rollAbs - s.rollEma) * alpha;

			int sign = (rollSigned > 0.15f) ? 1 : (rollSigned < -0.15f ? -1 : 0);
			bool signFlip = (sign != 0 && s.lastRollSign != 0 && sign != s.lastRollSign);

			bool rollNowOk =
				(s.rollEma >= min_roll_rate) &&
				(purity >= min_roll_purity) &&
				(!signFlip);

			if (rollNowOk) {
				s.rollStreak = std::min(streak_cap, s.rollStreak + 1);
				if (sign != 0) s.lastRollSign = sign;
			}
			else {
				s.rollStreak = std::max(0, s.rollStreak - 2);
				if (sign != 0) s.lastRollSign = sign;
			}

			// penalità NO-roll solo quando stai chiaramente andando verso la porta avversaria
			if (!rollNowOk && towardOppGoal > 0.35f && prox01 > 0.20f) {
				flyReward -= no_roll_pen_per_sec * dt;
			}

			// reward airroll solo dopo streak minimo
			if (s.rollStreak >= streak_needed) {
				float streak01 = clamp01((float)s.rollStreak / (float)streak_cap);
				float rate01 = clamp01(s.rollEma / target_roll_rate);

				float context = 0.55f + 0.25f * towardOppGoal + 0.20f * prox01;

				flyReward += airroll_per_sec * streak01 * (0.35f + 0.65f * rate01) * context * dt;
			}

			// ===== SCALING GLOBALE (auto -> porta avversaria) con minimo =====
			float goalMult = goal_scale_min +
				(1.0f - goal_scale_min) * std::pow(towardOppGoal, goal_scale_pow);

			flyReward *= goalMult;

			// somma (touch bonus + fly shaping scalato)
			reward += flyReward;

			if (debug) {
				std::cout << "[FlyToGoal] id=" << player.carId
					<< " dt=" << dt
					<< " touched=" << (touched ? 1 : 0)
					<< " touchBonus=" << ((touched && !player.isOnGround) ? air_touch_bonus : 0.0f)
					<< " dist=" << dist3
					<< " v2=" << v2len
					<< " tBall=" << towardBall
					<< " tOppGoal=" << towardOppGoal
					<< " prox=" << prox01
					<< " goalMult=" << goalMult
					<< " rollAbs=" << rollAbs
					<< " rollEma=" << s.rollEma
					<< " purity=" << purity
					<< " streak=" << s.rollStreak
					<< " flyR=" << flyReward
					<< " r=" << reward << "\n";
			}

			return reward;
		}
	};*/

	class ConcedeDistancePenalty : public Reward {
	public:
		// Magnitudine massima della penalità (quando sei ~alla massima distanza utile)
		float max_penalty = 1.0f;
		// Curvatura della penalità (1 = lineare; 2 = quadratica più severa da lontano)
		float curvature = 2.0f;

		float GetReward(const Player& player, const GameState& state, bool /*isFinal*/) override {
			// Trigger solo su evento goal
			if (!state.goalScored) return 0.0f;

			// Se la tua squadra ha subito (concede) il goal → applica penalità
			const bool scored_by_us = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y)); // come in GoalReward
			if (scored_by_us) return 0.0f;

			// Centro della propria porta
			const Vec ownGoal = (player.team == Team::BLUE)
				? CommonValues::BLUE_GOAL_CENTER
				: CommonValues::ORANGE_GOAL_CENTER;

			// Distanza 2D giocatore-porta
			const float dx = player.pos.x - ownGoal.x;
			const float dy = player.pos.y - ownGoal.y;
			const float dist2D = std::sqrt(dx * dx + dy * dy);

			// Lunghezza totale del campo (pareti di fondo ±BACK_WALL_Y)
			const float field_length = 2.0f * CommonValues::BACK_WALL_Y;

			// Normalizza in [0,1] e applica eventuale curvatura
			float t = (field_length > 1e-6f) ? std::min(1.0f, std::max(0.0f, dist2D / field_length)) : 0.0f;
			if (curvature > 1.0f) {
				t = std::pow(t, curvature);  // più lontano → più severo
			}

			// Penalità negativa (evento singolo)
			return -max_penalty * t;
		}
	};

	class TouchAccelReward : public Reward {
	public:
		constexpr static float MAX_REWARDED_BALL_SPEED = RLGC::Math::KPHToVel(110);

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev)
				return 0;

			if (player.ballTouchedStep) {
				float prevSpeedFrac = RS_MIN(1, state.prev->ball.vel.Length() / MAX_REWARDED_BALL_SPEED);
				float curSpeedFrac = RS_MIN(1, state.ball.vel.Length() / MAX_REWARDED_BALL_SPEED);

				if (curSpeedFrac > prevSpeedFrac) {
					return (curSpeedFrac - prevSpeedFrac);
				}
				else {
					// Not speeding up the ball so we don't care
					return 0;
				}
			}
			else {
				return 0;
			}
		}
	};

	class TeamSpacingReward : public Reward {
	public:
		float minSpacing;

		// minSpacing: Minimum distance between teammates (in Rocket League units)
		// Players get full reward when >= minSpacing apart, penalty when too close
		TeamSpacingReward(float minSpacing = 1500.0f) : minSpacing(minSpacing) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			float totalReward = 0.0f;
			int teammateCount = 0;

			// Check spacing with all teammates
			for (const Player& teammate : state.players) {
				// Skip self and opponents
				if (teammate.carId == player.carId || teammate.team != player.team)
					continue;

				teammateCount++;
				float distance = (player.pos - teammate.pos).Length();

				if (distance >= minSpacing) {
					// Good spacing - full reward
					totalReward += 1.0f;
				}
				else {
					// Too close - linear penalty based on how close they are
					float ratio = distance / minSpacing;  // 0 to 1
					totalReward += ratio;  // Linear reward from 0 to 1
				}
			}

			// Return average reward across all teammates (0 if no teammates)
			return teammateCount > 0 ? totalReward / teammateCount : 0.0f;
		}
	};


	class FlyToGoalKeepHigh : public Reward {
	public:
		// ============================================================================
		// PARAMETRI BASE
		// ============================================================================

		// --- Gating & reward base (per-tick) ---
		float min_air_height = 320.0f;
		float max_ball_dist = 650.0f;
		float per_tick_scale = 0.08f;

		// --- Touch bonus (esponenziale verso porta) ---
		float touch_base = 10.0f;
		float lambda_goal = 5.0f;
		float min_touch_factor = 0.0f;
		float max_delta_v = 2500.0f;
		bool  use_impulse_delta = true;

		// --- Air roll bonus (BILANCIATO) ---
		float airroll_bonus_base = 1.0f;
		float airroll_bonus_max = 5.0f;
		float min_airroll_rate = 0.05f;
		float airroll_speed_threshold = 150.0f;

		int   max_progression_ticks = 40;
		int   min_ticks_for_bonus = 4;

		int   decay_rate_active = 4;
		int   decay_rate_slow = 3;
		int   decay_rate_gating = 12;

		float smooth_factor_active = 0.70f;
		float smooth_factor_inactive = 0.40f;

		float quality_threshold = 0.8f;
		float quality_multiplier = 2.2f;
		float consistency_bonus_rate = 1.0f;
		int   consistency_start_ticks = 15;

		float direction_detect_mult = 2.0f;

		// --- Field position scaling ---
		float min_field_mult = 0.45f;
		float max_field_mult = 1.25f;
		float optimal_start_dist = 6000.0f;
		float too_far_penalty_dist = 10000.0f;
		bool  use_field_scaling = true;

		// --- Bonus "sotto la palla" (solo per touch) ---
		float under_ball_min_gap = 180.0f;
		float under_ball_max_gap = 450.0f;
		float under_touch_boost = 0.5f;

		// ============================================================================
		// PARAMETRI ALLINEAMENTO DIREZIONE TARGET
		// ============================================================================
		float w_dir_ball = 0.35f;
		float w_dir_goal = 0.75f;
		float min_goal_push_dot = 0.30f;
		float cross_track_penalty = 0.20f;
		float opposite_dir_penalty = 0.35f;

		// ============================================================================
		// PARAMETRI SHOT QUALITY
		// ============================================================================
		float shot_quality_bonus = 8.0f;
		float optimal_approach_angle = 25.0f;
		float shot_quality_distance_max = 0.8f;
		float optimal_shot_height = 380.0f;
		float shot_height_bonus = 4.0f;

		// ============================================================================
		// 🆕 OPTIMAL HEIGHT MAINTENANCE SYSTEM
		// ============================================================================
		float optimalHeight = 600.0f;              // Target height to maintain (uu)
		float optimalHeightTolerance = 50.0f;     // Tolerance range (+/- from optimal)
		float heightMaintenanceWeight = 0.2f;      // Weight for height maintenance reward
		float zVelMaintenanceWeight = 0.1f;        // Weight for Z velocity maintenance
		float targetZVel = 500.0f;                 // Target Z velocity (positive = rising, 0 = floating)
		float zVelTolerance = 300.0f;              // Tolerance for Z velocity
		float proximityWeight = 0.05f;              // Weight for proximity to ball
		float maxProximityDistance = 200.0f;       // Maximum distance for proximity bonus
		int   maxConsecutiveTouches = 20;          // Maximum consecutive touches tracked

		// Height relative to ball
		bool  useRelativeHeight = true;            // If true, optimal height is relative to ball
		float relativeHeightOffset = -80.0f;       // Offset from ball height (negative = below ball)
		float minAbsoluteHeight = 400.0f;          // Minimum absolute height even in relative mode

		// Height reward shaping
		float heightPenaltyScale = 0.3f;           // Penalty multiplier when outside optimal range
		float maxHeightDeviation = 500.0f;         // Max deviation before zero height reward
		float risingBonus = 0.1f;                  // Bonus for rising toward optimal height
		float maintainBonus = 0.2f;                // Bonus for maintaining optimal height

		// ============================================================================
		// STATE TRACKING
		// ============================================================================
		struct PlayerAirRollState {
			int   consecutive_ticks = 0;
			float prev_roll_direction = 0.0f;
			float smoothed_roll_rate = 0.0f;
			float accumulated_bonus = 0.0f;
			int   stable_direction_ticks = 0;
			float total_airroll_time = 0.0f;
			bool  has_reached_min_ticks = false;

			// Shot quality tracking
			float best_shot_quality = 0.0f;
			int   ticks_in_good_position = 0;

			// 🆕 Height maintenance tracking
			int   ticks_at_optimal_height = 0;
			float smoothed_z_vel = 0.0f;
			float prev_height = 0.0f;
			int   consecutive_touches = 0;
		};
		std::unordered_map<uint32_t, PlayerAirRollState> player_states;

		// ============================================================================
		// HELPER FUNCTIONS
		// ============================================================================

		inline static float clamp01(float x) {
			return std::max(0.0f, std::min(1.0f, x));
		}

		std::string GetName() override {
			return "FlyToGoalKeepHigh";
		}

		void Reset(const GameState& initialState) override {
			player_states.clear();
		}

		// 🆕 Calculate optimal target height (absolute or relative to ball)
		float GetTargetHeight(float ball_height) {
			if (useRelativeHeight) {
				float relative_target = ball_height + relativeHeightOffset;
				return std::max(minAbsoluteHeight, relative_target);
			}
			return optimalHeight;
		}

		// 🆕 Calculate height maintenance reward
		float GetHeightMaintenanceReward(const Player& player, float ball_height, PlayerAirRollState& ar_state) {
			float reward = 0.0f;
			float car_height = player.pos.z;
			float target_height = GetTargetHeight(ball_height);

			// Height deviation from optimal
			float height_diff = std::abs(car_height - target_height);

			// Smooth Z velocity tracking
			float current_z_vel = player.vel.z;
			ar_state.smoothed_z_vel = 0.8f * ar_state.smoothed_z_vel + 0.2f * current_z_vel;

			// ========================================
			// HEIGHT POSITION REWARD
			// ========================================
			if (height_diff <= optimalHeightTolerance) {
				// Within optimal range - full reward
				float height_quality = 1.0f - (height_diff / optimalHeightTolerance);
				reward += heightMaintenanceWeight * height_quality;
				ar_state.ticks_at_optimal_height++;

				// Milestone bonuses for maintaining height
				if (ar_state.ticks_at_optimal_height == 30) {
					reward += maintainBonus;
				}
				else if (ar_state.ticks_at_optimal_height == 60) {
					reward += maintainBonus * 1.5f;
				}
				else if (ar_state.ticks_at_optimal_height == 120) {
					reward += maintainBonus * 2.0f;
				}
			}
			else if (height_diff <= maxHeightDeviation) {
				// Outside tolerance but not too far - reduced reward
				float deviation_ratio = (height_diff - optimalHeightTolerance) /
					(maxHeightDeviation - optimalHeightTolerance);
				float height_quality = 1.0f - deviation_ratio;
				reward += heightMaintenanceWeight * height_quality * (1.0f - heightPenaltyScale);
				ar_state.ticks_at_optimal_height = 0;
			}
			else {
				// Too far from optimal - penalty
				reward -= heightMaintenanceWeight * heightPenaltyScale;
				ar_state.ticks_at_optimal_height = 0;
			}

			// ========================================
			// Z VELOCITY MAINTENANCE REWARD
			// ========================================
			float z_vel_diff = std::abs(ar_state.smoothed_z_vel - targetZVel);

			if (z_vel_diff <= zVelTolerance) {
				// Good Z velocity
				float z_vel_quality = 1.0f - (z_vel_diff / zVelTolerance);
				reward += zVelMaintenanceWeight * z_vel_quality;
			}
			else {
				// Z velocity too different from target
				float excess = z_vel_diff - zVelTolerance;
				float penalty = std::min(excess / 500.0f, 1.0f) * zVelMaintenanceWeight * 0.5f;
				reward -= penalty;
			}

			// ========================================
			// RISING TOWARD OPTIMAL BONUS
			// ========================================
			if (car_height < target_height - optimalHeightTolerance) {
				// Below optimal - reward rising
				if (current_z_vel > 100.0f) {
					float rising_quality = clamp01(current_z_vel / 800.0f);
					reward += risingBonus * rising_quality;
				}
			}
			else if (car_height > target_height + optimalHeightTolerance) {
				// Above optimal - reward controlled descent (not crashing)
				if (current_z_vel > -300.0f && current_z_vel < 100.0f) {
					reward += risingBonus * 0.5f;  // Controlled hover/slight descent
				}
			}

			ar_state.prev_height = car_height;
			return reward;
		}

		float GetFieldPositionMultiplier(const Player& player, const Vec& ball_pos) {
			if (!use_field_scaling) return 1.0f;

			Vec oppGoal = (player.team == RocketSim::Team::BLUE)
				? CommonValues::ORANGE_GOAL_CENTER
				: CommonValues::BLUE_GOAL_CENTER;
			Vec ownGoal = (player.team == RocketSim::Team::BLUE)
				? CommonValues::BLUE_GOAL_CENTER
				: CommonValues::ORANGE_GOAL_CENTER;

			float dist_to_opp_goal = (ball_pos - oppGoal).Length();
			float field_mult = 1.0f;

			if (dist_to_opp_goal <= optimal_start_dist) {
				float proximity_factor = 1.0f - (dist_to_opp_goal / optimal_start_dist);
				field_mult = 1.0f + (max_field_mult - 1.0f) * proximity_factor;
			}
			else if (dist_to_opp_goal <= too_far_penalty_dist) {
				float penalty_factor = (dist_to_opp_goal - optimal_start_dist) /
					(too_far_penalty_dist - optimal_start_dist);
				field_mult = 1.0f - (1.0f - min_field_mult) * penalty_factor;
			}
			else {
				field_mult = min_field_mult;
			}

			float midfield_y = (oppGoal.y + ownGoal.y) / 2.0f;
			bool in_own_half = (player.team == RocketSim::Team::BLUE && ball_pos.y < midfield_y) ||
				(player.team == RocketSim::Team::ORANGE && ball_pos.y > midfield_y);
			if (in_own_half) {
				field_mult *= 0.85f;
			}

			return std::max(min_field_mult, std::min(max_field_mult, field_mult));
		}

		float GetShotQuality(const Vec& player_pos, const Vec& ball_pos,
			const Vec& ball_to_goal_dir, float ball_height) {
			Vec player_to_ball = ball_pos - player_pos;
			player_to_ball.z = 0.0f;
			float dist = player_to_ball.Length();

			if (dist < 1e-6f) return 0.5f;

			Vec approach_dir = player_to_ball / dist;
			float dot = approach_dir.Dot(ball_to_goal_dir);
			dot = std::max(-1.0f, std::min(1.0f, dot));

			float angle_rad = std::acos(dot);
			float angle_deg = angle_rad * 180.0f / 3.14159265f;

			float angle_quality = 0.0f;
			if (angle_deg < optimal_approach_angle) {
				angle_quality = 1.0f;
			}
			else if (angle_deg < optimal_approach_angle + 40.0f) {
				float ratio = (angle_deg - optimal_approach_angle) / 40.0f;
				angle_quality = 1.0f - 0.6f * ratio;
			}
			else {
				angle_quality = 0.4f;
			}

			float height_diff = std::abs(ball_height - optimal_shot_height);
			float height_quality = 1.0f;
			if (height_diff > 400.0f) {
				height_quality = 0.5f;
			}
			else {
				height_quality = 1.0f - 0.5f * (height_diff / 400.0f);
			}

			return angle_quality * height_quality;
		}

		// ============================================================================
		// MAIN REWARD FUNCTION
		// ============================================================================

		float GetReward(const Player& player, const GameState& state, bool /*isFinal*/) override {
			auto& ar_state = player_states[player.carId];

			// ========================================================================
			// GATING
			// ========================================================================
			bool passed_gates = true;
			if (player.isOnGround || player.pos.z < min_air_height) {
				passed_gates = false;
			}

			float dist = (state.ball.pos - player.pos).Length();
			if (dist > max_ball_dist) {
				passed_gates = false;
			}

			if (!passed_gates) {
				ar_state.consecutive_ticks = std::max(0, ar_state.consecutive_ticks - decay_rate_gating);
				ar_state.stable_direction_ticks = std::max(0, ar_state.stable_direction_ticks - decay_rate_gating);
				ar_state.smoothed_roll_rate *= 0.4f;
				ar_state.has_reached_min_ticks = false;
				ar_state.best_shot_quality = 0.0f;
				ar_state.ticks_in_good_position = 0;
				ar_state.ticks_at_optimal_height = 0;
				ar_state.consecutive_touches = 0;
				return 0.0f;
			}

			float field_mult = GetFieldPositionMultiplier(player, state.ball.pos);

			// ========================================================================
			// 🆕 HEIGHT MAINTENANCE REWARD
			// ========================================================================
			float height_reward = GetHeightMaintenanceReward(player, state.ball.pos.z, ar_state);
			float reward = height_reward * field_mult;

			// ========================================================================
			// 🆕 PROXIMITY REWARD (staying close to ball)
			// ========================================================================
			if (dist < maxProximityDistance) {
				float proximity_quality = 1.0f - (dist / maxProximityDistance);
				reward += proximityWeight * proximity_quality * field_mult;
			}

			// ========================================================================
			// DIREZIONI BASE
			// ========================================================================
			Vec toBall = state.ball.pos - player.pos;
			Vec toBall2D(toBall.x, toBall.y, 0.0f);
			float d2 = toBall2D.Length();
			Vec dirToBall2D = (d2 > 1e-6f) ? (toBall2D / d2) : Vec(0, 0, 0);

			Vec v2D(player.vel.x, player.vel.y, 0.0f);
			float v2 = v2D.Length();
			Vec vHat2D = (v2 > 1e-6f) ? (v2D / v2) : Vec(0, 0, 0);

			// ========================================================================
			// DIREZIONE TARGET BLEND
			// ========================================================================
			Vec oppGoal = (player.team == RocketSim::Team::BLUE)
				? CommonValues::ORANGE_GOAL_CENTER
				: CommonValues::BLUE_GOAL_CENTER;

			Vec ballToGoal2D(oppGoal.x - state.ball.pos.x, oppGoal.y - state.ball.pos.y, 0.0f);
			float bgL = ballToGoal2D.Length();
			Vec dirBallToGoal2D = (bgL > 1e-6f) ? (ballToGoal2D / bgL) : Vec(0, 0, 0);

			float nearBall01 = clamp01((max_ball_dist - std::min(max_ball_dist, d2)) / max_ball_dist);
			float wB = w_dir_ball * (0.6f + 0.4f * nearBall01);
			float wG = w_dir_goal * (1.0f - 0.3f * nearBall01);

			Vec desired2D = wB * dirToBall2D + wG * dirBallToGoal2D;
			float desL = desired2D.Length();
			if (desL > 1e-6f) desired2D = desired2D / desL;

			Vec fwd2D(player.rotMat.forward.x, player.rotMat.forward.y, 0.0f);
			float f2 = fwd2D.Length();
			if (f2 > 1e-6f) fwd2D = fwd2D / f2;

			float align_vel_desired = clamp01(vHat2D.Dot(desired2D));
			float align_fwd_desired = clamp01(fwd2D.Dot(desired2D));

			float cross = std::sqrt(std::max(0.0f, 1.0f - align_vel_desired * align_vel_desired));
			float cross_pen = cross_track_penalty * cross;

			float opposite_pen = 0.0f;
			if (v2 > 50.0f) {
				float rawDot = vHat2D.Dot(desired2D);
				if (rawDot < 0.0f) {
					opposite_pen = opposite_dir_penalty * (-rawDot);
				}
			}

			// ========================================================================
			// ALLINEAMENTI CLASSICI
			// ========================================================================
			float vlen = player.vel.Length();
			Vec vHat = (vlen > 1e-6f) ? (player.vel / vlen) : Vec(0, 0, 0);
			Vec dirToBall = (dist > 1e-6f) ? ((state.ball.pos - player.pos) / dist) : Vec(0, 0, 0);
			float align_ball = clamp01(vHat.Dot(dirToBall));

			const float R = CommonValues::BALL_RADIUS;
			float proximity = clamp01(1.0f - (dist - R) / std::max(1.0f, (max_ball_dist - R)));

			// ========================================================================
			// REWARD BASE PER-TICK
			// ========================================================================
			float shaped_core =
				0.50f * align_vel_desired +
				0.20f * align_fwd_desired +
				0.20f * align_ball +
				0.10f * proximity;

			shaped_core = clamp01(shaped_core);
			reward += per_tick_scale * shaped_core * field_mult;

			reward -= (cross_pen + opposite_pen) * field_mult;

			// ========================================================================
			// SHOT QUALITY BONUS
			// ========================================================================
			if (dist < max_ball_dist * shot_quality_distance_max) {
				float shot_quality = GetShotQuality(player.pos, state.ball.pos,
					dirBallToGoal2D, state.ball.pos.z);

				reward += shot_quality_bonus * shot_quality * field_mult * 0.04f;

				ar_state.best_shot_quality = std::max(ar_state.best_shot_quality, shot_quality);

				if (shot_quality > 0.75f) {
					ar_state.ticks_in_good_position++;

					if (ar_state.ticks_in_good_position == 15) {
						reward += 2.0f * field_mult;
					}
					else if (ar_state.ticks_in_good_position == 30) {
						reward += 3.0f * field_mult;
					}
				}
				else {
					ar_state.ticks_in_good_position = 0;
				}

				float height_diff = std::abs(state.ball.pos.z - optimal_shot_height);
				if (height_diff < 150.0f) {
					float height_quality = 1.0f - (height_diff / 150.0f);
					reward += shot_height_bonus * height_quality * field_mult * 0.03f;
				}
			}

			// ========================================================================
			// TOUCH BONUS
			// ========================================================================
			if (player.ballTouchedStep) {
				ar_state.consecutive_touches = std::min(ar_state.consecutive_touches + 1, maxConsecutiveTouches);

				Vec dV = (use_impulse_delta && state.prev)
					? (state.ball.vel - state.prev->ball.vel)
					: state.ball.vel;

				Vec dV2D(dV.x, dV.y, 0.0f);
				float dVlen = dV2D.Length();

				if (dVlen > 1e-6f && bgL > 1e-6f) {
					Vec dVhat = dV2D / dVlen;
					float dot_goal = std::max(-1.0f, std::min(1.0f, dVhat.Dot(dirBallToGoal2D)));

					float e_pos = std::exp(lambda_goal * dot_goal);
					float e_min = std::exp(-lambda_goal);
					float e_max = std::exp(lambda_goal);
					float gain01 = (e_pos - e_min) / (e_max - e_min);

					float mag = clamp01(dVlen / std::max(1.0f, max_delta_v));

					float dir_factor = (dot_goal > 0.0f) ? gain01 : 0.0f;

					float touch = touch_base * mag * dir_factor * field_mult;

					// 🆕 Consecutive touch bonus (air dribble)
					if (ar_state.consecutive_touches > 1) {
						float touch_streak_bonus = 1.0f + 0.1f * std::min(ar_state.consecutive_touches - 1, 10);
						touch *= touch_streak_bonus;
					}

					if (dot_goal > 0.5f) {
						float vertical_gap = state.ball.pos.z - player.pos.z;
						if (vertical_gap > under_ball_min_gap && dV.z > 0.0f) {
							float gap01 = clamp01((vertical_gap - under_ball_min_gap) /
								std::max(1.0f, under_ball_max_gap - under_ball_min_gap));
							float touch_boost = 1.0f + under_touch_boost * gap01;
							touch *= touch_boost;
						}
					}

					reward += touch;

					if (dot_goal > 0.9f && mag > 0.6f) {
						reward += 5.0f * field_mult;
					}
				}
			}
			else {
				// Decay consecutive touches if no touch this tick
				// (keep it if still in air and close to ball)
				if (dist > maxProximityDistance * 1.5f) {
					ar_state.consecutive_touches = 0;
				}
			}

			// ========================================================================
			// AIR ROLL BONUS
			// ========================================================================
			if (vlen > airroll_speed_threshold && align_vel_desired > min_goal_push_dot) {
				Vec forward = player.rotMat.forward;
				float roll_rate_signed = player.angVel.Dot(forward);
				float roll_rate_abs = std::fabs(roll_rate_signed);

				float smooth_factor = (roll_rate_abs > min_airroll_rate)
					? smooth_factor_active
					: smooth_factor_inactive;
				ar_state.smoothed_roll_rate = smooth_factor * ar_state.smoothed_roll_rate +
					(1.0f - smooth_factor) * roll_rate_abs;

				float direction_threshold = min_airroll_rate * direction_detect_mult;
				float curr_direction = 0.0f;
				if (roll_rate_signed > direction_threshold) curr_direction = 1.0f;
				if (roll_rate_signed < -direction_threshold) curr_direction = -1.0f;

				bool direction_changed = false;
				if (curr_direction != 0.0f && ar_state.prev_roll_direction != 0.0f) {
					if (curr_direction * ar_state.prev_roll_direction < 0) {
						direction_changed = true;
					}
				}

				if (ar_state.smoothed_roll_rate > min_airroll_rate && !direction_changed) {
					ar_state.consecutive_ticks++;
					ar_state.stable_direction_ticks++;
					ar_state.total_airroll_time += 1.0f;

					if (ar_state.stable_direction_ticks >= min_ticks_for_bonus) {
						ar_state.has_reached_min_ticks = true;
					}

					if (ar_state.has_reached_min_ticks) {
						float progression = std::min(
							static_cast<float>(ar_state.stable_direction_ticks) /
							static_cast<float>(max_progression_ticks),
							1.0f
						);

						float current_bonus = airroll_bonus_base +
							(airroll_bonus_max - airroll_bonus_base) * progression;

						float intensity_factor = clamp01(ar_state.smoothed_roll_rate / 1.0f);
						intensity_factor = std::max(0.5f, intensity_factor);

						float speed_factor = clamp01(vlen / 1800.0f);
						speed_factor = 0.6f + 0.4f * speed_factor;

						float proximity_factor = clamp01(proximity * 2.0f);
						proximity_factor = 0.7f + 0.3f * proximity_factor;

						float consistency_factor = 1.0f;
						if (ar_state.stable_direction_ticks > consistency_start_ticks) {
							float consistency_progress = std::min(
								static_cast<float>(ar_state.stable_direction_ticks - consistency_start_ticks) / 80.0f,
								1.0f
							);
							consistency_factor = 1.0f + consistency_bonus_rate * consistency_progress;
						}

						float desired_factor = 0.7f + 0.3f * align_vel_desired;

						float avg_quality = (intensity_factor + speed_factor + proximity_factor) / 3.0f;
						float quality_mult = (avg_quality > quality_threshold) ? quality_multiplier : 1.0f;

						float airroll_bonus = current_bonus *
							intensity_factor *
							speed_factor *
							proximity_factor *
							consistency_factor *
							desired_factor *
							quality_mult *
							field_mult;

						ar_state.accumulated_bonus += airroll_bonus;
						reward += airroll_bonus;

						if (ar_state.stable_direction_ticks == 60 ||
							ar_state.stable_direction_ticks == 120) {
							reward += 0.5f * field_mult;
						}
					}
				}
				else {
					if (direction_changed) {
						ar_state.consecutive_ticks = 0;
						ar_state.stable_direction_ticks = 0;
						ar_state.accumulated_bonus = 0.0f;
						ar_state.has_reached_min_ticks = false;
					}
					else if (ar_state.smoothed_roll_rate <= min_airroll_rate) {
						ar_state.consecutive_ticks = std::max(0, ar_state.consecutive_ticks - decay_rate_active);
						ar_state.stable_direction_ticks = std::max(0, ar_state.stable_direction_ticks - decay_rate_active);

						if (ar_state.stable_direction_ticks < min_ticks_for_bonus) {
							ar_state.has_reached_min_ticks = false;
						}
					}
				}

				if (curr_direction != 0.0f) {
					ar_state.prev_roll_direction = curr_direction;
				}

			}
			else {
				ar_state.consecutive_ticks = std::max(0, ar_state.consecutive_ticks - 1);
				ar_state.stable_direction_ticks = std::max(0, ar_state.stable_direction_ticks - decay_rate_slow);
				ar_state.smoothed_roll_rate *= 0.75f;

				if (ar_state.stable_direction_ticks < min_ticks_for_bonus) {
					ar_state.has_reached_min_ticks = false;
				}
			}

			return reward;
		}
	};


	class DoubleTapReward : public Reward {
	public:
		struct Params {
			// Step 1: Primo tocco aereo - Rewards
			float firstTouchReward = 5.0f;
			float strongFlickBonus = 50.0f;
			float directionToBackboardBonus = 80.0f;

			// Step 1: Penalties
			float heightPenaltyScale = 5.0f;

			// Step 2: Rimbalzo backboard - Rewards
			float backboardBounceReward = 100.0f;
			float alignmentBonus = 40.0f;
			float stayAirborneBonus = 20.0f;

			// Step 2: Penalties
			float groundTouchPenalty = 10.0f;
			float tooFarPenalty = 10.0f;
			float tooClosePenalty = 30.0f;
			float misalignmentPenalty = 30.0f;

			// Step 3: Secondo tocco - Rewards
			float secondTouchReward = 100.0f;
			float goalDirectionBonus = 80.0f;
			float goalPotentialBonus = 60.0f;

			// Step 3: Penalties
			float notAirbornePenalty = 20.0f;
			float wrongDirectionPenalty = 20.0f;

			// Step 5: Goal finale
			float goalReward = 300.0f;
			float velocityBonusScale = 1.0f;

			// Moltiplicatore di sequenza
			float sequenceMultiplier = 1.5f;

			// Thresholds Step 1
			float minFirstTouchHeight = 250.0f;
			float minStrongFlickSpeed = 1200.0f;
			float optimalBallHeight = 600.0f;
			float minBallHeight = 300.0f;
			float maxBallHeight = 1600.0f;

			// Thresholds Step 2
			float backboardYThreshold = 4800.0f;
			float backboardZThreshold = CommonValues::GOAL_HEIGHT;
			float optimalDistance = 1000.0f;
			float minDistance = 600.0f;
			float maxDistance = 1500.0f;
			float trajectoryRadius = 350.0f;

			// Thresholds Step 3
			float goalDirectionAngleMax = 45.0f;
			float goalPotentialSpeed = 800.0f;
			float touchRadius = 250.0f;

			// General
			float maxTimeBetweenSteps = 4.0f;
			int tickSkip = 8;
		} params;

	private:
		enum class SequenceState {
			IDLE,
			FIRST_AERIAL_TOUCH,   // Step 1
			BACKBOARD_BOUNCE,      // Step 2
			SECOND_TOUCH,          // Step 3
			GOAL_SCORED            // Step 5
		};

		struct AgentState {
			SequenceState state = SequenceState::IDLE;
			int completedSteps = 0;

			Vec lastBallPos = Vec(0, 0, 0);
			Vec ballVelBeforeTouch = Vec(0, 0, 0);
			Vec ballVelAfterTouch = Vec(0, 0, 0);
			Vec ballVelAtBounce = Vec(0, 0, 0);

			float timeSinceLastStep = 0.0f;
			float distanceAtBounce = 0.0f;
			int targetBackboardSign = 0;

			bool wasAirborne = false;
			bool groundedDuringStep2 = false;

			void Reset() {
				state = SequenceState::IDLE;
				completedSteps = 0;
				lastBallPos = Vec(0, 0, 0);
				ballVelBeforeTouch = Vec(0, 0, 0);
				ballVelAfterTouch = Vec(0, 0, 0);
				ballVelAtBounce = Vec(0, 0, 0);
				timeSinceLastStep = 0.0f;
				distanceAtBounce = 0.0f;
				targetBackboardSign = 0;
				wasAirborne = false;
				groundedDuringStep2 = false;
			}
		};

		std::unordered_map<uint32_t, AgentState> agentStates;
		Vec lastBallVelocity = Vec(0, 0, 0);
		uint32_t lastTouchPlayerId = UINT32_MAX;

		static inline int BackboardSignForTeam(RocketSim::Team team) {
			return (team == RocketSim::Team::BLUE) ? +1 : -1;
		}

		static inline float Clamp01(float x) {
			return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
		}

		// Calcola quanto bene la palla va verso la backboard (meglio) o porta (ok)
		float CalculateBackboardDirectionScore(const Vec& ballPos, const Vec& ballVel,
			int targetSign, const Params& p) const {
			float velLen = ballVel.Length();
			if (velLen < 1e-3f) return 0.0f;
			Vec velNorm = ballVel / velLen;

			// Target ideale: backboard centrata sulla porta
			Vec backboardTarget(0.0f, targetSign * p.backboardYThreshold,
				p.backboardZThreshold * 0.7f);
			Vec toBB = backboardTarget - ballPos;
			float distToBB = toBB.Length();
			if (distToBB < 1e-3f) return 1.0f;
			toBB = toBB / distToBB;

			float dotBackboard = velNorm.Dot(toBB);

			// Alternativa: se va direttamente in porta va bene lo stesso (ma meno)
			Vec goalTarget(0.0f, targetSign * CommonValues::BACK_WALL_Y, 100.0f);
			Vec toGoal = goalTarget - ballPos;
			float distToGoal = toGoal.Length();
			if (distToGoal > 1e-3f) {
				toGoal = toGoal / distToGoal;
				float dotGoal = velNorm.Dot(toGoal);
				// Backboard è meglio (100%), porta diretta è ok (70%)
				return std::max(Clamp01(dotBackboard), Clamp01(dotGoal * 0.7f));
			}

			return Clamp01(dotBackboard);
		}

		// Calcola penalità per altezza palla (troppo alta o troppo bassa)
		float CalculateHeightPenalty(float ballZ, const Params& p) const {
			if (ballZ >= p.minBallHeight && ballZ <= p.maxBallHeight) {
				// Dentro range accettabile
				float distFromOptimal = std::abs(ballZ - p.optimalBallHeight);
				return distFromOptimal / 200.0f; // Penalità graduale
			}

			// Fuori range
			if (ballZ < p.minBallHeight) {
				float deficit = p.minBallHeight - ballZ;
				return deficit / 50.0f; // Penalità più forte
			}
			else {
				float excess = ballZ - p.maxBallHeight;
				return excess / 50.0f;
			}
		}

		// Calcola allineamento macchina con traiettoria palla
		float CalculateAlignment(const Vec& carPos, const Vec& carVel,
			const Vec& ballPos, const Vec& ballVel,
			const Params& p) const {
			Vec carToBall = ballPos - carPos;
			float dist = carToBall.Length();
			if (dist < 1e-3f) return 1.0f;
			carToBall = carToBall / dist;

			// Direzione della palla
			float ballSpeed = ballVel.Length();
			if (ballSpeed < 100.0f) return 0.3f;
			Vec ballDir = ballVel / ballSpeed;

			// La macchina dovrebbe essere "dietro" la palla rispetto alla direzione
			// dot positivo = macchina è nella direzione da cui arriva la palla
			float positionDot = (-carToBall).Dot(ballDir);

			// Velocità macchina dovrebbe essere allineata con direzione palla
			float carSpeed = carVel.Length();
			float velocityAlignment = 0.5f;
			if (carSpeed > 200.0f) {
				Vec carDir = carVel / carSpeed;
				velocityAlignment = (carDir.Dot(ballDir) + 1.0f) * 0.5f; // map [-1,1] to [0,1]
			}

			// Combina allineamento posizione e velocità
			return Clamp01((positionDot * 0.6f + velocityAlignment * 0.4f));
		}

		// Verifica se la macchina è in traiettoria del pallone
		bool IsInTrajectory(const Vec& carPos, const Vec& ballPos,
			const Vec& ballVel, float radius) const {
			float ballSpeed = ballVel.Length();
			if (ballSpeed < 100.0f) return false;
			Vec ballDir = ballVel / ballSpeed;

			// Distanza perpendicolare dalla linea di traiettoria
			Vec toBall = ballPos - carPos;
			float alongTrajectory = toBall.Dot(ballDir);

			if (alongTrajectory < -200.0f) return false; // Troppo indietro

			Vec projectionPoint = carPos + ballDir * alongTrajectory;
			float perpDist = (ballPos - projectionPoint).Length();

			return perpDist <= radius;
		}

		// Calcola score per direzione verso porta
		float CalculateGoalDirectionScore(const Vec& ballPos, const Vec& ballVel,
			int targetSign, const Params& p) const {
			float velLen = ballVel.Length();
			if (velLen < 100.0f) return 0.0f;
			Vec velNorm = ballVel / velLen;

			Vec goalPos(0.0f, targetSign * CommonValues::BACK_WALL_Y, 100.0f);
			Vec toGoal = goalPos - ballPos;
			float distToGoal = toGoal.Length();
			if (distToGoal < 1e-3f) return 1.0f;
			toGoal = toGoal / distToGoal;

			float dot = velNorm.Dot(toGoal);
			float angleDeg = std::acos(std::max(-1.0f, std::min(1.0f, dot))) * 180.0f / 3.14159265f;

			if (angleDeg <= p.goalDirectionAngleMax) {
				return 1.0f - (angleDeg / p.goalDirectionAngleMax) * 0.5f;
			}
			return 0.0f;
		}

		// Valuta se il tiro è un possibile goal
		float EvaluateGoalPotential(const Vec& ballPos, const Vec& ballVel,
			int targetSign, const Params& p) const {
			// 1. Velocità sufficiente
			float speed = ballVel.Length();
			float speedScore = std::min(1.0f, speed / p.goalPotentialSpeed);

			// 2. Direzione verso porta
			float dirScore = CalculateGoalDirectionScore(ballPos, ballVel, targetSign, p);

			// 3. Altezza ragionevole per entrare in porta
			float heightScore = 1.0f;
			if (ballPos.z > CommonValues::GOAL_HEIGHT * 1.2f) {
				heightScore = 0.4f; // Troppo alto
			}
			else if (ballPos.z < 100.0f) {
				heightScore = 0.6f; // Troppo basso
			}

			// 4. Distanza ragionevole
			float distToGoal = std::abs(std::abs(ballPos.y) - CommonValues::BACK_WALL_Y);
			float distScore = 1.0f;
			if (distToGoal > 3000.0f) {
				distScore = 0.7f; // Molto lontano
			}

			return (speedScore * 0.4f + dirScore * 0.4f + heightScore * 0.1f + distScore * 0.1f);
		}

	public:
		DoubleTapReward() = default;

		void Reset(const GameState& initialState) override {
			agentStates.clear();
			lastBallVelocity = initialState.ball.vel;
			lastTouchPlayerId = UINT32_MAX;
		}

		void PreStep(const GameState& state) override {
			lastBallVelocity = state.ball.vel;
		}

		float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			float reward = 0.0f;
			AgentState& S = agentStates[player.carId];
			const float dt = params.tickSkip / 120.0f;

			const Vec ballPos = state.ball.pos;
			const Vec ballVel = state.ball.vel;
			const float distToBall = (player.pos - ballPos).Length();
			const int teamTargetSign = BackboardSignForTeam(player.team);

			if (S.state != SequenceState::IDLE) {
				S.timeSinceLastStep += dt;
			}

			if (player.ballTouchedStep) {
				lastTouchPlayerId = player.carId;
				S.ballVelBeforeTouch = lastBallVelocity;
				S.ballVelAfterTouch = ballVel;
			}

			const bool nearBackboard =
				(std::abs(ballPos.y) > params.backboardYThreshold) &&
				(ballPos.z > params.backboardZThreshold);

			auto moveTowardTarget = [&](const Vec& v) -> bool {
				return (teamTargetSign > 0) ? (v.y > 0.0f) : (v.y < 0.0f);
				};

			auto vyFlipFromTarget = [&](const Vec& vPrev, const Vec& vNow) -> bool {
				bool prevToward = moveTowardTarget(vPrev);
				bool nowAway = !moveTowardTarget(vNow);
				return (prevToward && nowAway);
				};

			switch (S.state) {
			case SequenceState::IDLE: {
				// ========== STEP 1: PRIMO TOCCO AEREO ==========
				if (player.ballTouchedStep && !player.isOnGround &&
					ballPos.z >= params.minFirstTouchHeight) {

					S.state = SequenceState::FIRST_AERIAL_TOUCH;
					S.completedSteps = 1;
					S.targetBackboardSign = teamTargetSign;
					S.timeSinceLastStep = 0.0f;
					S.wasAirborne = true;

					// Reward base
					reward += params.firstTouchReward;

					// BONUS: Secondo tocco in aria forte (flick o supersonic)
					float speedAfter = ballVel.Length();
					if (speedAfter >= params.minStrongFlickSpeed || player.isSupersonic) {
						reward += params.strongFlickBonus;
					}

					// BONUS: Direzione verso backboard (meglio) o porta (ok)
					float dirScore = CalculateBackboardDirectionScore(
						ballPos, ballVel, teamTargetSign, params);
					reward += params.directionToBackboardBonus * dirScore;

					// MALUS: Pallone troppo alto o troppo basso
					float heightPenalty = CalculateHeightPenalty(ballPos.z, params);
					reward -= params.heightPenaltyScale * heightPenalty;
				}
				break;
			}

			case SequenceState::FIRST_AERIAL_TOUCH: {
				// Aspetta il rimbalzo dalla backboard
				if (nearBackboard && vyFlipFromTarget(lastBallVelocity, ballVel)) {
					// ========== STEP 2: RIMBALZO BACKBOARD ==========
					S.state = SequenceState::BACKBOARD_BOUNCE;
					S.completedSteps = 2;
					S.timeSinceLastStep = 0.0f;
					S.distanceAtBounce = distToBall;
					S.ballVelAtBounce = ballVel;
					S.groundedDuringStep2 = player.isOnGround;

					float seqMult = std::pow(params.sequenceMultiplier, S.completedSteps - 1);

					// BONUS: Palla rimbalza indietro (già verificato)
					reward += params.backboardBounceReward * seqMult;

					// BONUS: Macchina allineata al vettore del pallone
					float alignment = CalculateAlignment(
						player.pos, player.vel, ballPos, ballVel, params);
					reward += params.alignmentBonus * alignment * seqMult;

					// BONUS: Né pallone né macchina toccano terra
					if (!player.isOnGround) {
						reward += params.stayAirborneBonus * seqMult;
					}

					// MALUS: Macchina troppo lontana
					if (distToBall > params.maxDistance) {
						float excess = distToBall - params.maxDistance;
						float penalty = std::min(1.0f, excess / 500.0f);
						reward -= params.tooFarPenalty * penalty * seqMult;
					}

					// MALUS: Macchina troppo vicina
					if (distToBall < params.minDistance) {
						float deficit = params.minDistance - distToBall;
						float penalty = std::min(1.0f, deficit / 300.0f);
						reward -= params.tooClosePenalty * penalty * seqMult;
					}

					// MALUS: Non si trova in traiettoria del pallone
					if (!IsInTrajectory(player.pos, ballPos, ballVel, params.trajectoryRadius)) {
						reward -= params.misalignmentPenalty * seqMult;
					}
				}

				// Traccia se tocca terra
				if (player.isOnGround && S.wasAirborne) {
					S.groundedDuringStep2 = true;
				}

				// Timeout
				if (S.timeSinceLastStep > params.maxTimeBetweenSteps) {
					S.Reset();
				}
				break;
			}

			case SequenceState::BACKBOARD_BOUNCE: {
				// ========== STEP 3: SECONDO TOCCO ==========

				// MALUS (piccolo): Macchina ha toccato terra durante o prima del rimbalzo
				if (player.isOnGround && !S.groundedDuringStep2) {
					float seqMult = std::pow(params.sequenceMultiplier, S.completedSteps - 1);
					reward -= params.groundTouchPenalty * seqMult;
					S.groundedDuringStep2 = true;
				}

				if (player.ballTouchedStep && distToBall <= params.touchRadius) {
					// MALUS forte: Non si è in volo durante l'inizio dello step 3
					if (player.isOnGround) {
						reward -= params.notAirbornePenalty;
						S.Reset();
						break;
					}

					S.state = SequenceState::SECOND_TOUCH;
					S.completedSteps = 3;
					S.timeSinceLastStep = 0.0f;

					float seqMult = std::pow(params.sequenceMultiplier, S.completedSteps - 1);

					// BONUS: Avviene in aria (già verificato)
					reward += params.secondTouchReward * seqMult;

					// BONUS: Pallone va in direzione della porta
					float dirScore = CalculateGoalDirectionScore(
						ballPos, ballVel, S.targetBackboardSign, params);

					if (dirScore > 0.5f) {
						reward += params.goalDirectionBonus * dirScore * seqMult;
					}
					else {
						// MALUS: Pallone non è in direzione della porta
						reward -= params.wrongDirectionPenalty * (1.0f - dirScore) * seqMult;
					}

					// BONUS: Il tiro è un possibile goal
					float potential = EvaluateGoalPotential(
						ballPos, ballVel, S.targetBackboardSign, params);
					reward += params.goalPotentialBonus * potential * seqMult;
				}

				// Timeout
				if (S.timeSinceLastStep > params.maxTimeBetweenSteps) {
					S.Reset();
				}
				break;
			}

			case SequenceState::SECOND_TOUCH: {
				// ========== STEP 5: GOAL! ==========
				if (state.goalScored && lastTouchPlayerId == player.carId) {
					S.state = SequenceState::GOAL_SCORED;
					S.completedSteps = 4;

					float seqMult = std::pow(params.sequenceMultiplier, S.completedSteps - 1);

					// Reward base goal
					reward += params.goalReward * seqMult;

					// Bonus aggiuntivo per velocità del tiro
					float ballSpeed = lastBallVelocity.Length();
					float velocityBonus = (ballSpeed / 3000.0f) * params.velocityBonusScale * params.goalReward;
					reward += velocityBonus * seqMult;

					S.Reset();
				}

				// Timeout
				if (S.timeSinceLastStep > params.maxTimeBetweenSteps) {
					S.Reset();
				}
				break;
			}

			case SequenceState::GOAL_SCORED:
				// Sequenza completata!
				break;
			}

			// Traccia stato airborne per il prossimo tick
			S.wasAirborne = !player.isOnGround;
			S.lastBallPos = ballPos;

			return reward;
		}
	};

	class WasteBoostPenalty : public Reward {
	public:
		// COSTANTI NOTE
		static constexpr float BOOST_ACCEL = 991.666f;     // uu/s^2
		static constexpr float TICK_HZ = 120.0f;
		static constexpr float DT = 1.0f / TICK_HZ;
		static constexpr float BOOST_DV_TICK = BOOST_ACCEL * DT; // 8.263883

		// TUNABLE
		float min_spend = 0.2f;     // considera "boosting" se consuma almeno così in un tick
		float ratio_min = 0.70f;    // richiedi almeno 70% dell'atteso (margine errori)
		float abs_margin = 1.0f;    // uu/s per tick: tolleranza assoluta extra (rumore/curve/collisioncine)
		float k = 0.02f;            // scala penalità
		float speed_eps = 10.0f;    // evita rumore quando quasi fermo

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			float prevBoost = player.prev->boost;
			float curBoost = player.boost;

			float spent = std::max(0.0f, prevBoost - curBoost);
			if (spent < min_spend) return 0.0f;

			Vec vprev = player.prev->vel;
			Vec vcur = player.vel;

			float sp = vprev.Length();
			float sc = vcur.Length();

			// soglia minima di Δv lungo direzione di moto, con margine
			const float dv_min = BOOST_DV_TICK * ratio_min - abs_margin;

			if (sp < speed_eps) {
				// Da quasi fermo: guardo incremento di speed (non ho direzione affidabile)
				float ds = sc - sp;
				if (ds < dv_min) return -k * spent;
				return 0.0f;
			}

			Vec vhat = vprev / sp;
			Vec dv = vcur - vprev;

			float dv_along = dv.Dot(vhat); // incremento velocità nella direzione di moto (per tick)

			// Se rallenta mentre consuma boost => spreco quasi sicuro
			if (dv_along < 0.0f) return -k * spent;

			// Se accelera meno del minimo (70% - margine) => spreco/inefficienza
			if (dv_along < dv_min) return -k * spent;

			return 0.0f;
		}
	};



	class PickupBoostReward : public Reward {
	public:
		// TUNABLE
		float small_pad_pay = 12.0f;
		float big_pad_pay = 100.0f;
		float small_pad_max_delta = 12.5f; // tolleranza float (12 o meno)

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			const float prevBoost = player.prev->boost;
			const float curBoost = player.boost;
			const float delta = curBoost - prevBoost;
			

			if (delta <= 0.0f) return 0.0f;

			// Classifica l'evento come pad piccolo o grosso in base al salto
			if (delta <= small_pad_max_delta) return small_pad_pay;
			return big_pad_pay;
		}
	};



	class MustyFlickAfterResetGoalReward : public Reward {
	public:
		// =========================
		// TUNABLES
		// =========================
		float min_air_height = 220.0f;
		float max_ball_dist = 750.0f;

		// Shaping dense (per tick) quando sei in finestra musty
		float per_tick_scale = 0.06f;

		// "Sotto la palla" target gap (palla sopra l'auto)
		float under_gap_min = 60.0f;
		float under_gap_max = 260.0f;

		// Richiedi che il bot abbia speso il flip dopo reset e tocchi entro questa finestra
		uint64_t musty_touch_window_ticks = 360; // ~3s @120Hz

		// Backflip detection (pitch rate)
		float min_pitch_rate = 2.5f;   // rad/s approx
		int   flip_pitch_sign = -1;    // prova -1 o +1 in base al tuo asse (vedi note sotto)
		float flip_detect_bonus = 0.35f;

		// Touch quality (impulso sulla palla)
		float min_dv = 250.0f;         // minimo delta-v palla per considerarlo "flick"
		float max_dv = 2200.0f;        // clamp per quality
		float min_up_dv = 120.0f;      // deve "scoopare" almeno un po' verso l'alto

		// Goal bonus (goal normale = 400 già altrove)
		float musty_goal_bonus = 260.0f;
		uint64_t goal_window_ticks = 420; // ~3.5s

		// Debug
		bool debug = false;

	private:
		struct St {
			bool armed = false;              // abbiamo un flip reset "attivo"
			uint64_t arm_tick = 0;

			bool flip_spent = false;         // ho speso il flip dopo reset
			uint64_t flip_spent_tick = 0;

			// info ultimo touch musty-like
			bool musty_touch = false;
			float last_musty_quality = 0.0f;
			uint64_t musty_touch_tick = 0;

			// per goal bonus
			bool goal_armed = false;
			uint64_t goal_arm_tick = 0;

			// tick tracking
			uint64_t prev_tick = 0;
		};

		std::unordered_map<uint32_t, St> st_;

		inline static float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }
		static Team TeamFromGoalY(float y) { return (y > 0.0f) ? Team::ORANGE : Team::BLUE; }

		uint64_t GetTick(const GameState& state) const {
			return state.lastArena ? state.lastArena->tickCount : state.lastTickCount;
		}

		// quality helper: map [a..b] to [0..1]
		static float ramp01(float x, float a, float b) {
			if (b <= a) return (x >= a) ? 1.0f : 0.0f;
			return clamp01((x - a) / (b - a));
		}

	public:
		std::string GetName() override { return "MustyFlickAfterResetGoalReward"; }

		void Reset(const GameState&) override { st_.clear(); }

		float GetReward(const Player& player, const GameState& state, bool /*isFinal*/) override {
			auto& s = st_[player.carId];

			const uint64_t tick = GetTick(state);

			// tickskip-safe stepTicks (se ti serve scalarlo)
			if (s.prev_tick == 0) s.prev_tick = tick;
			uint64_t stepTicks = (tick > s.prev_tick) ? (tick - s.prev_tick) : 1;
			stepTicks = std::min<uint64_t>(stepTicks, 24);
			s.prev_tick = tick;

			// =========================
			// GOAL BONUS (prima)
			// =========================
			if (state.goalScored && s.goal_armed) {
				// finestra
				if (tick - s.goal_arm_tick <= goal_window_ticks) {
					const bool scored_by_us = (player.team != TeamFromGoalY(state.ball.pos.y));
					if (scored_by_us) {
						float bonus = musty_goal_bonus * clamp01(s.last_musty_quality);
						// pulisci (sequenza conclusa)
						s = St{};
						s.prev_tick = tick;
						return bonus;
					}
				}
				// scaduta / goal contro / etc
				s.goal_armed = false;
			}

			// =========================
			// GATING BASE
			// =========================
			if (player.isOnGround) {
				// se tocchi terra, resettiamo la sequenza musty
				s.armed = false;
				s.flip_spent = false;
				s.musty_touch = false;
				s.goal_armed = false;
				s.last_musty_quality = 0.0f;
				return 0.0f;
			}

			if (player.pos.z < min_air_height) return 0.0f;

			const float dist = (state.ball.pos - player.pos).Length();
			if (dist > max_ball_dist) return 0.0f;

			// =========================
			// ARM: hai flip reset
			// =========================
			const bool has_reset_now = player.HasFlipReset();
			const bool prev_has_reset = (player.prev != nullptr) ? player.prev->HasFlipReset() : false;

			// Armo quando ottieni il reset (transizione) O quando sei già in reset e non armato
			if ((!prev_has_reset && has_reset_now) || (has_reset_now && !s.armed)) {
				s.armed = true;
				s.arm_tick = tick;

				s.flip_spent = false;
				s.musty_touch = false;
				s.last_musty_quality = 0.0f;
			}

			if (!s.armed) return 0.0f;

			// Timeout arm (evita che resti armato per sempre)
			if (tick - s.arm_tick > 2 * musty_touch_window_ticks) {
				s.armed = false;
				s.flip_spent = false;
				return 0.0f;
			}

			// =========================
			// Rileva "flip speso" dopo reset
			// =========================
			const bool has_flip_or_jump_now = player.HasFlipOrJump();
			const bool prev_has_flip_or_jump = (player.prev != nullptr) ? player.prev->HasFlipOrJump() : true;

			if (!s.flip_spent && prev_has_flip_or_jump && !has_flip_or_jump_now) {
				s.flip_spent = true;
				s.flip_spent_tick = tick;
			}

			// Se ho speso il flip ma non tocco entro finestra -> disarmo
			if (s.flip_spent && (tick - s.flip_spent_tick > musty_touch_window_ticks)) {
				s.armed = false;
				s.flip_spent = false;
				return 0.0f;
			}

			// =========================
			// Continuous shaping (dense)
			// =========================
			Vec oppGoal = (player.team == Team::BLUE) ? CommonValues::ORANGE_GOAL_CENTER : CommonValues::BLUE_GOAL_CENTER;

			Vec ballToGoal = oppGoal - state.ball.pos;
			float bgL = ballToGoal.Length();
			Vec dirBallToGoal = (bgL > 1e-6f) ? (ballToGoal / bgL) : Vec(0, 0, 0);

			// "sotto la palla": gap positivo se palla sopra auto
			float gap = state.ball.pos.z - player.pos.z;
			float under01 = ramp01(gap, under_gap_min, under_gap_max); // cresce se sei sotto con gap decente

			// allineamento velocità auto verso porta (2D) (solo shaping)
			Vec v2(player.vel.x, player.vel.y, 0.0f);
			float v2L = v2.Length();
			Vec v2Hat = (v2L > 1e-6f) ? (v2 / v2L) : Vec(0, 0, 0);
			Vec g2(dirBallToGoal.x, dirBallToGoal.y, 0.0f);
			float g2L = g2.Length();
			Vec g2Hat = (g2L > 1e-6f) ? (g2 / g2L) : Vec(0, 0, 0);
			float alignGoal01 = clamp01(v2Hat.Dot(g2Hat));

			// bonus "sto flippando" e con pitch da backflip (euristico)
			float backflip01 = 0.0f;
			{
				// richiede che rotMat.right esista
				float pitchRate = player.angVel.Dot(player.rotMat.right);
				// backflip circa: segno coerente + magnitudine
				float signedRate = (float)flip_pitch_sign * pitchRate;
				backflip01 = ramp01(signedRate, min_pitch_rate, min_pitch_rate * 2.2f);
			}

			// shaping per tick, più forte dopo che hai speso il flip (per guidare il timing)
			float phase01 = s.flip_spent ? 1.0f : 0.45f;
			float dense = per_tick_scale * phase01 * (0.55f * under01 + 0.45f * alignGoal01);

			// se sta riconoscendo backflip, spingi un pelo (ma non troppo)
			dense *= (1.0f + flip_detect_bonus * backflip01);

			float reward = dense * (float)stepTicks;

			// =========================
			// Touch detection & musty-like touch quality
			// =========================
			if (player.ballTouchedStep && state.prev) {
				// solo se è una touch dopo aver speso il flip (sennò è spesso “primo touch reset”)
				if (s.flip_spent) {
					Vec dV = state.ball.vel - state.prev->ball.vel;

					// quanto la palla va verso porta avversaria
					float bSpeed = state.ball.vel.Length();
					Vec bDir = (bSpeed > 1e-6f) ? (state.ball.vel / bSpeed) : Vec(0, 0, 0);
					float dir01 = clamp01(bDir.Dot(dirBallToGoal));

					// impulso utile (2D + verticale)
					float dvMag = dV.Length();
					float dv01 = ramp01(dvMag, min_dv, max_dv);

					float up01 = ramp01(dV.z, min_up_dv, 900.0f);

					// richiedi anche “sotto la palla” al momento del touch
					float mustyQ = clamp01(dv01 * (0.35f + 0.65f * dir01) * (0.35f + 0.65f * under01) * (0.25f + 0.75f * up01));

					// piccola dipendenza dal backflip detection (non hard-gate, per renderla learnable)
					mustyQ *= (0.65f + 0.35f * backflip01);

					// reward sul touch (continuous-ish: proporzionale a qualità)
					reward += 140.0f * mustyQ; // << qui è “il cuore”: abbastanza grande da emergere, ma non > goal

					// arma goal bonus
					s.musty_touch = (mustyQ > 0.25f);
					s.last_musty_quality = mustyQ;
					s.musty_touch_tick = tick;

					if (s.musty_touch) {
						s.goal_armed = true;
						s.goal_arm_tick = tick;
					}

					// disarma la parte “reset->flip speso” (sequenza finita, ora aspettiamo goal)
					s.armed = false;
					s.flip_spent = false;
				}
			}

			return reward;
		}
	};


	class PopResetReward : public Reward {
	public:
		explicit PopResetReward(
			float scale = 1.0f,
			float flipAfterResetWindowSec = 0.45f,
			float maxArmedWaitSec = 4.0f,
			int   maxTouchAttempts = 7,
			float minUpwardVelZGain = 30.0f,
			float minDeltaVNorm = 150.0f,
			float deltaVelScale = 1.0f,
			float zSqWeight = 2.0f,
			bool  onlyUpwardZ = true,
			float popSaturation = 200000.0f,

			float mult1 = 1.0f,
			float mult2 = 8.0f,
			float mult3 = 16.0f,
			float mult4 = 2.0f,
			float mult5Plus = 2.0f,

			float groundMaxCarZ = 170.0f,
			float wallNearDist = 250.0f,
			float wallMaxCarZ = 380.0f,
			float maxAbsVzForGround = 350.0f,
			float groundGraceSec = 0.03f,

			float minAirZForReset = 220.0f,

			float angleStartDeg = 35.0f,
			float angleEndDeg = 60.0f,
			float angleMinFactor = 0.1f,
			bool  angleSmoothStep = true,

			float xyKmhMin = 10.0f,
			float xyKmhMax = 60.0f,
			float xyMultMin = 0.2f,
			float xyMultMax = 1.0f,

			float heightMultAtLow = 0.2f,
			float heightMultAtHigh = 1.0f,
			float heightLowFracGoal = 2.0f / 3.0f,
			float heightHighFracCeil = 0.55f,
			float disableHeightNearGoalXYDist = 1400.0f,

			// kept in signature for backwards compat, ignored internally
			float /*wastedPopPenalty*/ = 0.0f,
			float /*wastedPopMaxWatchSec*/ = 0.0f,
			float /*wastedPopGroundDiamMult*/ = 0.0f,
			float /*disableWastedPopNearGoalXYDist*/ = 2500.0f,
			float /*ballWallTouchDist_waste*/ = 120.0f,
			float /*ballWallTouchHyst_waste*/ = 30.0f,

			// ===== Field-position multiplier =====
			float ownBackboardMult = 0.3f,
			float midlineMult = 0.7f,
			float enemyHalfMidlineMult = 1.0f,
			float enemyGoalCloseMult = 0.1f,
			float enemyGoalCloseBallDiam = 3.5f,

			// ===== No-bounce goal bonus =====
			float noBounceGoalBonus = 2.0f,
			float noBounceMaxWatchSec = 6.0f,
			float noBounceGroundDiamMult = 1.8f,
			float noBounceWallTouchDist = 120.0f,
			float noBounceWallTouchHyst = 30.0f,
			int   noBounceMinPopCount = 2,

			// kept in signature for backwards compat, ignored internally
			bool  /*penalizeSuccessInOwnGoalSideFrac*/ = true,
			float /*ownGoalSideFracOfHalf*/ = 2.0f / 5.0f,
			float /*ownGoalSideSuccessPenalty*/ = 8.35f
		)
			: scale(scale),
			flipAfterResetWindowSec(std::max(0.01f, flipAfterResetWindowSec)),
			maxArmedWaitSec(std::max(0.05f, maxArmedWaitSec)),
			maxTouchAttempts(std::max(1, maxTouchAttempts)),
			minUpwardVelZGain(minUpwardVelZGain),
			minDeltaVNorm(std::max(0.0f, minDeltaVNorm)),
			deltaVelScale(deltaVelScale),
			zSqWeight(zSqWeight),
			onlyUpwardZ(onlyUpwardZ),
			popSaturation(std::max(1.0f, popSaturation)),
			mult1(mult1), mult2(mult2), mult3(mult3), mult4(mult4), mult5Plus(mult5Plus),
			groundMaxCarZ(groundMaxCarZ),
			wallNearDist(wallNearDist),
			wallMaxCarZ(std::max(groundMaxCarZ, wallMaxCarZ)),
			maxAbsVzForGround(std::max(0.0f, maxAbsVzForGround)),
			groundGraceSec(std::max(0.0f, groundGraceSec)),
			minAirZForReset(std::max(0.0f, minAirZForReset)),
			angleStartDeg(std::max(0.0f, angleStartDeg)),
			angleEndDeg(std::max(angleStartDeg + 0.1f, angleEndDeg)),
			angleMinFactor(std::clamp(angleMinFactor, 0.0f, 1.0f)),
			angleSmoothStep(angleSmoothStep),
			xyKmhMin(std::max(0.0f, xyKmhMin)),
			xyKmhMax(std::max(xyKmhMin + 0.1f, xyKmhMax)),
			xyMultMin(std::clamp(xyMultMin, 0.0f, 10.0f)),
			xyMultMax(std::clamp(xyMultMax, 0.0f, 10.0f)),
			heightMultAtLow(std::max(0.0f, heightMultAtLow)),
			heightMultAtHigh(std::max(0.0f, heightMultAtHigh)),
			heightLowFracGoal(std::max(0.0f, heightLowFracGoal)),
			heightHighFracCeil(std::max(0.0f, heightHighFracCeil)),
			disableHeightNearGoalXYDist(std::max(0.0f, disableHeightNearGoalXYDist)),
			ownBackboardMult(std::max(0.0f, ownBackboardMult)),
			midlineMult(std::max(0.0f, midlineMult)),
			enemyHalfMidlineMult(std::max(0.0f, enemyHalfMidlineMult)),
			enemyGoalCloseMult(std::max(0.0f, enemyGoalCloseMult)),
			enemyGoalCloseBallDiam(std::max(0.0f, enemyGoalCloseBallDiam)),
			noBounceGoalBonus(std::max(0.0f, noBounceGoalBonus)),
			noBounceMaxWatchSec(std::max(0.05f, noBounceMaxWatchSec)),
			noBounceGroundDiamMult(std::max(0.1f, noBounceGroundDiamMult)),
			noBounceWallTouchDist(std::max(0.0f, noBounceWallTouchDist)),
			noBounceWallTouchHyst(std::max(0.0f, noBounceWallTouchHyst)),
			noBounceMinPopCount(std::max(1, noBounceMinPopCount))
		{
		}

		void Reset(const GameState&) override {
			states.clear();
		}

		float GetReward(const Player& player, const GameState& state, bool) override {
			auto& s = states[player.carId];
			const float dt = (state.deltaTime > 0.f) ? state.deltaTime : (1.f / 120.f);

			// --- Goal edge detection ---
			const bool goalNow = state.goalScored;
			s.goalEdge = (!s.prevGoalScored && goalNow);
			s.prevGoalScored = goalNow;

			float reward = 0.f;

			// --- No-bounce goal bonus (must run before ground resets chain) ---
			reward += TickNoBounceWatch(player, state, dt, s);

			// --- Ground detection resets aerial chain ---
			const bool grounded = IsGrounded(player, dt, s.groundGrace);
			if (grounded) {
				s.successCount = 0;
				s.pending = false;
				s.armed = false;
				s.pendingTimer = 0.f;
				s.armedTimer = 0.f;
				s.touchAttempts = 0;
				s.sawFlipAvail = false;
				s.flipUsed = false;
			}

			// --- Edge detectors for flip-reset acquisition ---
			const bool hasFlipNow = player.HasFlipOrJump();
			const bool gotResetNow = player.GotFlipReset();

			const bool gotResetEdge = (!s.prevGotReset && gotResetNow);
			s.prevGotReset = gotResetNow;

			const bool flipRegainedEdge = (!s.prevHasFlip && hasFlipNow);
			s.prevHasFlip = hasFlipNow;

			const bool airborneEnough = (!grounded && player.pos.z >= minAirZForReset);
			const bool resetAcquired = airborneEnough && (flipRegainedEdge || gotResetEdge);

			// --- State machine: IDLE → PENDING → ARMED → SUCCESS ---

			if (resetAcquired) {
				s.pending = true;
				s.pendingTimer = 0.f;
				s.sawFlipAvail = hasFlipNow;
				s.flipUsed = false;
				s.armed = false;
				s.armedTimer = 0.f;
				s.touchAttempts = 0;

				if (gotResetEdge && !hasFlipNow) {
					s.flipUsed = true;
				}
			}

			if (s.pending) {
				s.pendingTimer += dt;

				if (hasFlipNow) s.sawFlipAvail = true;
				if (s.sawFlipAvail && !hasFlipNow) s.flipUsed = true;

				if (s.flipUsed && s.pendingTimer <= flipAfterResetWindowSec) {
					s.armed = true;
					s.pending = false;
					s.armedTimer = 0.f;
					s.touchAttempts = 0;
				}

				if (s.pendingTimer > flipAfterResetWindowSec) {
					s.pending = false;
				}
			}

			if (s.armed) {
				s.armedTimer += dt;

				if (s.armedTimer > maxArmedWaitSec) {
					s.armed = false;
					s.armedTimer = 0.f;
					s.touchAttempts = 0;
				}
				else if (player.ballTouchedStep && state.prev) {
					s.touchAttempts += 1;

					const Vec& vb = state.prev->ball.vel;
					const Vec& va = state.ball.vel;

					const float dvx = va.x - vb.x;
					const float dvy = va.y - vb.y;
					const float dvz = va.z - vb.z;
					const float dvNorm = std::sqrt(dvx * dvx + dvy * dvy + dvz * dvz);

					const bool success = (dvz >= minUpwardVelZGain) && (dvNorm >= minDeltaVNorm);

					if (success) {
						float dzUse = onlyUpwardZ ? std::max(dvz, 0.f) : dvz;
						const float popScore = dvx * dvx + dvy * dvy + zSqWeight * dzUse * dzUse;
						const float popRatio = popScore / (popScore + popSaturation);

						const float chainMult = GetChainMult(s.successCount);
						float successReward = popRatio * deltaVelScale * chainMult;

						successReward *= GetGoalAngleFactor(player, state);
						successReward *= GetXySpeedFactor(state);
						successReward *= GetHeightFactor(player, state);
						successReward *= GetFieldPosMult(player, state);

						reward += successReward;

						// No-bounce watcher (only if chain qualifies)
						const int newCount = s.successCount + 1;
						if (newCount >= noBounceMinPopCount) {
							StartNoBounceWatch(state, s);
						}
						else {
							s.nbWatch = false;
							s.nbValid = false;
							s.nbTimer = 0.f;
						}

						// Same-tick goal
						if (s.goalEdge && state.goalScored
							&& newCount >= noBounceMinPopCount
							&& s.nbValid && IsOppGoalScored(player, state))
						{
							reward += noBounceGoalBonus;
							s.nbWatch = false;
							s.nbValid = false;
							s.nbTimer = 0.f;
						}

						s.successCount = newCount;
						s.armed = false;
						s.armedTimer = 0.f;
						s.touchAttempts = 0;
					}
					else {
						if (s.touchAttempts >= maxTouchAttempts) {
							s.armed = false;
							s.armedTimer = 0.f;
							s.touchAttempts = 0;
						}
					}
				}
			}

			// --- Final: guaranteed non-negative, NaN-safe ---
			float out = reward * scale;
			if (!std::isfinite(out)) out = 0.f;
			return std::max(0.f, out);
		}

	private:
		struct PerCarState {
			int  successCount = 0;

			bool prevHasFlip = false;
			bool prevGotReset = false;

			bool  pending = false;
			float pendingTimer = 0.f;
			bool  sawFlipAvail = false;
			bool  flipUsed = false;

			bool  armed = false;
			float armedTimer = 0.f;
			int   touchAttempts = 0;

			float groundGrace = 0.f;

			bool  prevGoalScored = false;
			bool  goalEdge = false;

			// no-bounce goal watcher
			bool  nbWatch = false;
			bool  nbValid = false;
			float nbTimer = 0.f;
			float nbPrevBallWallDist = 999999.f;
		};

		// =====================================================================
		// Helpers
		// =====================================================================

		static float Saturate01(float x) {
			if (x <= 0.f) return 0.f;
			if (x >= 1.f) return 1.f;
			return x;
		}

		static float Length2D(const Vec& v) {
			return std::sqrt(v.x * v.x + v.y * v.y);
		}

		float GetChainMult(int count) const {
			if (count <= 0) return mult1;
			if (count == 1) return mult2;
			if (count == 2) return mult3;
			if (count == 3) return mult4;
			return mult5Plus;
		}

		Vec OppGoalCenter(const Player& player) const {
			return (player.team == RocketSim::Team::BLUE)
				? CommonValues::ORANGE_GOAL_CENTER
				: CommonValues::BLUE_GOAL_CENTER;
		}

		bool IsNearOppGoalXY(const Player& player, const GameState& state, float dist) const {
			Vec d = state.ball.pos - OppGoalCenter(player);
			d.z = 0.f;
			return Length2D(d) <= dist;
		}

		bool IsOppGoalScored(const Player& player, const GameState& state) const {
			return (state.ball.pos.y * OppGoalCenter(player).y) > 0.f;
		}

		// =====================================================================
		// Ground detection
		// =====================================================================
		bool IsGrounded(const Player& player, float dt, float& grace) const {
			float wallDistX = CommonValues::SIDE_WALL_X - std::abs(player.pos.x);
			float wallDistY = CommonValues::BACK_WALL_Y - std::abs(player.pos.y);
			float wallDist = std::min(wallDistX, wallDistY);
			wallDist = std::max(0.f, wallDist);

			const bool vzOk = (std::abs(player.vel.z) <= maxAbsVzForGround);
			const bool lowZ = (player.pos.z <= groundMaxCarZ);
			const bool nearWall = (wallDist <= wallNearDist) && (player.pos.z <= wallMaxCarZ);
			const bool groundNow = vzOk && (lowZ || nearWall);

			if (groundNow) grace = groundGraceSec;
			else grace = std::max(0.f, grace - dt);

			return (grace > 0.f);
		}

		// =====================================================================
		// Multiplier functions
		// =====================================================================
		float GetGoalAngleFactor(const Player& player, const GameState& state) const {
			const Vec oppGoal = OppGoalCenter(player);

			Vec goalDir = oppGoal - state.ball.pos;
			const float goalLen = goalDir.Length();
			if (goalLen <= 1e-3f) return 1.0f;
			goalDir /= goalLen;

			Vec vel = state.ball.vel;
			const float velLen = vel.Length();
			if (velLen <= 1e-3f) return 1.0f;
			vel /= velLen;

			float cosAng = std::clamp(vel.Dot(goalDir), -1.0f, 1.0f);
			const float angDeg = std::acos(cosAng) * (180.0f / 3.14159265f);

			if (angDeg <= angleStartDeg) return 1.0f;
			if (angDeg >= angleEndDeg)   return angleMinFactor;

			float t = (angDeg - angleStartDeg) / (angleEndDeg - angleStartDeg);
			t = Saturate01(t);
			if (angleSmoothStep) t = t * t * (3.f - 2.f * t);

			const float factor = 1.0f + (angleMinFactor - 1.0f) * t;
			return std::clamp(factor, angleMinFactor, 1.0f);
		}

		float GetXySpeedFactor(const GameState& state) const {
			const float UU_PER_SEC_TO_KMH = 0.036f;
			const float vx = state.ball.vel.x;
			const float vy = state.ball.vel.y;
			const float vxy_kmh = std::sqrt(vx * vx + vy * vy) * UU_PER_SEC_TO_KMH;

			const float v = std::clamp(vxy_kmh, xyKmhMin, xyKmhMax);
			const float denom = std::max(1e-3f, (xyKmhMax - xyKmhMin));

			float t = (v - xyKmhMin) / denom;
			t = Saturate01(t);

			const float factor = xyMultMin + (xyMultMax - xyMultMin) * std::sqrt(t);
			return std::clamp(factor, std::min(xyMultMin, xyMultMax), std::max(xyMultMin, xyMultMax));
		}

		float GetHeightFactor(const Player& player, const GameState& state) const {
			if (IsNearOppGoalXY(player, state, disableHeightNearGoalXYDist)) return 1.0f;

			const float z = std::clamp(state.ball.pos.z, 0.f, CommonValues::CEILING_Z);
			const float zLow = std::max(0.f, CommonValues::GOAL_HEIGHT * heightLowFracGoal);
			const float zHigh = std::max(zLow + 1.f, CommonValues::CEILING_Z * heightHighFracCeil);

			if (z <= zLow)  return heightMultAtLow;
			if (z >= zHigh) return heightMultAtHigh;

			const float t = (z - zLow) / (zHigh - zLow);
			return std::max(0.f, heightMultAtLow + (heightMultAtHigh - heightMultAtLow) * Saturate01(t));
		}

		float GetFieldPosMult(const Player& player, const GameState& state) const {
			const float ballDiam = 2.0f * CommonValues::BALL_RADIUS;
			const float nearOppGoalDist = enemyGoalCloseBallDiam * ballDiam;

			if (nearOppGoalDist > 0.f && IsNearOppGoalXY(player, state, nearOppGoalDist)) {
				return enemyGoalCloseMult;
			}

			const float Yb = CommonValues::BACK_WALL_Y;
			if (Yb <= 1e-3f) return 1.0f;

			const float signedY = (player.team == RocketSim::Team::BLUE)
				? state.ball.pos.y : -state.ball.pos.y;

			if (signedY < 0.f) {
				// Own half: ownBackboardMult → midlineMult
				float t = (signedY + Yb) / Yb;
				t = Saturate01(t);
				return ownBackboardMult + (midlineMult - ownBackboardMult) * t;
			}

			// Enemy half first segment: midlineMult → enemyHalfMidlineMult
			const float yHalfMid = 0.5f * Yb;
			if (signedY <= yHalfMid && yHalfMid > 1e-3f) {
				float t = signedY / yHalfMid;
				t = Saturate01(t);
				return midlineMult + (enemyHalfMidlineMult - midlineMult) * t;
			}

			return enemyHalfMidlineMult;
		}

		// =====================================================================
		// No-bounce goal bonus
		// =====================================================================
		void StartNoBounceWatch(const GameState& state, PerCarState& s) const {
			s.nbWatch = true;
			s.nbValid = true;
			s.nbTimer = 0.f;
			s.nbPrevBallWallDist = ComputeBallWallDist(state.ball.pos);
		}

		float TickNoBounceWatch(const Player& player, const GameState& state, float dt, PerCarState& s) const {
			if (!s.nbWatch) return 0.f;

			if (s.goalEdge) {
				float bonus = 0.f;
				if (s.nbValid && IsOppGoalScored(player, state)) {
					bonus = noBounceGoalBonus;
				}
				s.nbWatch = false;
				s.nbValid = false;
				s.nbTimer = 0.f;
				return bonus;
			}

			s.nbTimer += dt;
			if (s.nbTimer > noBounceMaxWatchSec) {
				s.nbWatch = false;
				s.nbValid = false;
				s.nbTimer = 0.f;
				return 0.f;
			}

			if (!s.nbValid) return 0.f;

			// Invalidate on wall touch (hysteresis)
			const float wallDistNow = ComputeBallWallDist(state.ball.pos);
			const bool wallTouchish =
				(wallDistNow <= noBounceWallTouchDist) &&
				(s.nbPrevBallWallDist > (noBounceWallTouchDist + noBounceWallTouchHyst));
			s.nbPrevBallWallDist = wallDistNow;

			if (wallTouchish) {
				s.nbValid = false;
				return 0.f;
			}

			// Invalidate on ground touch
			const float ballDiam = 2.0f * CommonValues::BALL_RADIUS;
			const float zThr = noBounceGroundDiamMult * ballDiam;
			if (state.ball.pos.z <= zThr) {
				s.nbValid = false;
				return 0.f;
			}

			return 0.f;
		}

		float ComputeBallWallDist(const Vec& ballPos) const {
			float wallDistX = CommonValues::SIDE_WALL_X - std::abs(ballPos.x);
			float wallDistY = CommonValues::BACK_WALL_Y - std::abs(ballPos.y);
			return std::max(0.f, std::min(wallDistX, wallDistY));
		}

		// =====================================================================
		// Parameters
		// =====================================================================
		float scale;

		float flipAfterResetWindowSec;
		float maxArmedWaitSec;
		int   maxTouchAttempts;

		float minUpwardVelZGain;
		float minDeltaVNorm;

		float deltaVelScale;
		float zSqWeight;
		bool  onlyUpwardZ;
		float popSaturation;

		float mult1, mult2, mult3, mult4, mult5Plus;

		float groundMaxCarZ;
		float wallNearDist;
		float wallMaxCarZ;
		float maxAbsVzForGround;
		float groundGraceSec;

		float minAirZForReset;

		float angleStartDeg;
		float angleEndDeg;
		float angleMinFactor;
		bool  angleSmoothStep;

		float xyKmhMin;
		float xyKmhMax;
		float xyMultMin;
		float xyMultMax;

		float heightMultAtLow;
		float heightMultAtHigh;
		float heightLowFracGoal;
		float heightHighFracCeil;
		float disableHeightNearGoalXYDist;

		float ownBackboardMult;
		float midlineMult;
		float enemyHalfMidlineMult;
		float enemyGoalCloseMult;
		float enemyGoalCloseBallDiam;

		float noBounceGoalBonus;
		float noBounceMaxWatchSec;
		float noBounceGroundDiamMult;
		float noBounceWallTouchDist;
		float noBounceWallTouchHyst;
		int   noBounceMinPopCount;

		std::unordered_map<uint32_t, PerCarState> states;
	};

	class PopResetDoubleTapReward : public Reward {
	public:
		explicit PopResetDoubleTapReward(
			float scale = 1.0f,

			// === Flip-reset detection ===
			float minAirZForReset = 220.0f,

			// === Ground detection (resets entire chain) ===
			float groundMaxCarZ = 170.0f,
			float wallNearDist = 250.0f,
			float wallMaxCarZ = 380.0f,
			float maxAbsVzForGround = 350.0f,
			float groundGraceSec = 0.03f,

			// === Backboard hit detection ===
			float backboardBallWallDist = 180.0f,
			float backboardMinBallZ = 300.0f,
			float backboardHitReward = 50.0f,

			// === Per-tick height matching ===
			float heightMatchRewardPerTick = 1.5f,
			float heightDeadzone = 200.0f,
			float heightFalloffRange = 200.0f,

			// === Per-tick X proximity (lateral, tight) ===
			float xProximityRewardPerTick = 0.5f,
			float xMaxDist = 400.0f,

			// === Per-tick Y proximity (depth, band / sweet-spot) ===
			//  Full reward inside [yIdealMin, yIdealMax].
			//  Linear falloff toward 0 if too close or too far.
			float yProximityRewardPerTick = 0.5f,
			float yIdealMin = 150.0f,
			float yIdealMax = 700.0f,
			float yCloseFalloff = 150.0f,
			float yFarFalloff = 400.0f,

			// === Second touch (the finishing tap) ===
			float secondTouchReward = 100.0f,
			float directionBonusMax = 100.0f,
			float speedBonusMax = 100.0f,
			float speedBonusMinKmh = 20.0f,
			float speedBonusMaxKmh = 120.0f,

			// === Goal scored bonus ===
			float goalScoredBonus = 200.0f,

			// === Phase timeouts ===
			float maxAerialWaitSec = 4.0f,
			float maxFinishingWaitSec = 3.0f
		)
			: scale(scale),
			minAirZForReset(std::max(0.0f, minAirZForReset)),
			groundMaxCarZ(groundMaxCarZ),
			wallNearDist(wallNearDist),
			wallMaxCarZ(std::max(groundMaxCarZ, wallMaxCarZ)),
			maxAbsVzForGround(std::max(0.0f, maxAbsVzForGround)),
			groundGraceSec(std::max(0.0f, groundGraceSec)),
			backboardBallWallDist(std::max(0.0f, backboardBallWallDist)),
			backboardMinBallZ(std::max(0.0f, backboardMinBallZ)),
			backboardHitReward(std::max(0.0f, backboardHitReward)),
			heightMatchRewardPerTick(std::max(0.0f, heightMatchRewardPerTick)),
			heightDeadzone(std::max(0.0f, heightDeadzone)),
			heightFalloffRange(std::max(1.0f, heightFalloffRange)),
			xProximityRewardPerTick(std::max(0.0f, xProximityRewardPerTick)),
			xMaxDist(std::max(1.0f, xMaxDist)),
			yProximityRewardPerTick(std::max(0.0f, yProximityRewardPerTick)),
			yIdealMin(std::max(0.0f, yIdealMin)),
			yIdealMax(std::max(yIdealMin + 1.0f, yIdealMax)),
			yCloseFalloff(std::max(1.0f, yCloseFalloff)),
			yFarFalloff(std::max(1.0f, yFarFalloff)),
			secondTouchReward(std::max(0.0f, secondTouchReward)),
			directionBonusMax(std::max(0.0f, directionBonusMax)),
			speedBonusMax(std::max(0.0f, speedBonusMax)),
			speedBonusMinKmh(std::max(0.0f, speedBonusMinKmh)),
			speedBonusMaxKmh(std::max(speedBonusMinKmh + 0.1f, speedBonusMaxKmh)),
			goalScoredBonus(std::max(0.0f, goalScoredBonus)),
			maxAerialWaitSec(std::max(0.1f, maxAerialWaitSec)),
			maxFinishingWaitSec(std::max(0.1f, maxFinishingWaitSec))
		{
		}

		void Reset(const GameState&) override {
			states.clear();
		}

		float GetReward(const Player& player, const GameState& state, bool) override {
			auto& s = states[player.carId];
			const float dt = (state.deltaTime > 0.f) ? state.deltaTime : (1.f / 120.f);

			float reward = 0.f;

			// -----------------------------------------------------------------
			// Goal edge detection
			// -----------------------------------------------------------------
			const bool goalNow = state.goalScored;
			const bool goalEdge = (!s.prevGoalScored && goalNow);
			s.prevGoalScored = goalNow;

			// -----------------------------------------------------------------
			// Ground detection -> resets entire chain
			// -----------------------------------------------------------------
			const bool grounded = IsGrounded(player, dt, s.groundGrace);
			if (grounded) {
				s.phase = Phase::IDLE;
				s.phaseTimer = 0.f;
				s.prevHasFlip = player.HasFlipOrJump();
				s.prevGotReset = player.GotFlipReset();
				s.prevBallNearBackboard = IsBallNearOppBackboard(player, state);
				return 0.f;
			}

			// -----------------------------------------------------------------
			// Flip-reset edge detectors
			// -----------------------------------------------------------------
			const bool hasFlipNow = player.HasFlipOrJump();
			const bool gotResetNow = player.GotFlipReset();

			const bool gotResetEdge = (!s.prevGotReset && gotResetNow);
			const bool flipRegainedEdge = (!s.prevHasFlip && hasFlipNow);
			s.prevGotReset = gotResetNow;
			s.prevHasFlip = hasFlipNow;

			const bool airborneEnough = (player.pos.z >= minAirZForReset);
			const bool resetAcquired = airborneEnough && (flipRegainedEdge || gotResetEdge);

			// -----------------------------------------------------------------
			// Backboard edge detection
			// -----------------------------------------------------------------
			const bool bbNow = IsBallNearOppBackboard(player, state);
			const bool bbEdge = (bbNow && !s.prevBallNearBackboard);
			s.prevBallNearBackboard = bbNow;

			// =================================================================
			// State machine: IDLE -> AERIAL -> FINISHING -> GOAL_WATCH
			// =================================================================

			switch (s.phase) {

				// ----- IDLE: waiting for any flip reset -----
			case Phase::IDLE:
				if (resetAcquired) {
					s.phase = Phase::AERIAL;
					s.phaseTimer = 0.f;
				}
				break;

				// ----- AERIAL: reset acquired, tracking ball toward backboard -----
			case Phase::AERIAL:
				s.phaseTimer += dt;

				// Per-tick tracking rewards
				reward += ComputeHeightMatchReward(player, state) * dt;
				reward += ComputeXProximityReward(player, state) * dt;
				reward += ComputeYProximityReward(player, state) * dt;

				// Backboard hit
				if (bbEdge) {
					reward += backboardHitReward;
					s.phase = Phase::FINISHING;
					s.phaseTimer = 0.f;
				}

				// Timeout
				if (s.phaseTimer > maxAerialWaitSec) {
					s.phase = Phase::IDLE;
				}

				// Direct goal while in aerial (ball goes in without bouncing)
				if (goalEdge && IsOppGoalScored(player, state)) {
					reward += goalScoredBonus;
					s.phase = Phase::IDLE;
				}
				break;

				// ----- FINISHING: backboard bounced, waiting for second touch -----
			case Phase::FINISHING:
				s.phaseTimer += dt;

				// Per-tick tracking rewards
				reward += ComputeHeightMatchReward(player, state) * dt;
				reward += ComputeXProximityReward(player, state) * dt;
				reward += ComputeYProximityReward(player, state) * dt;

				// Second touch
				if (player.ballTouchedStep && state.prev) {
					reward += secondTouchReward;
					reward += ComputeDirectionBonus(player, state);
					reward += ComputeSpeedBonus(state);

					if (goalEdge && IsOppGoalScored(player, state)) {
						reward += goalScoredBonus;
					}

					s.phase = Phase::GOAL_WATCH;
					s.phaseTimer = 0.f;
				}

				// Goal without explicit second touch
				if (goalEdge && IsOppGoalScored(player, state) && s.phase == Phase::FINISHING) {
					reward += goalScoredBonus;
					s.phase = Phase::IDLE;
				}

				// Timeout
				if (s.phaseTimer > maxFinishingWaitSec && s.phase == Phase::FINISHING) {
					s.phase = Phase::IDLE;
				}
				break;

				// ----- GOAL_WATCH: second touch done, brief window for goal -----
			case Phase::GOAL_WATCH:
				s.phaseTimer += dt;

				if (goalEdge && IsOppGoalScored(player, state)) {
					reward += goalScoredBonus;
					s.phase = Phase::IDLE;
				}

				if (s.phaseTimer > 3.0f) {
					s.phase = Phase::IDLE;
				}
				break;
			}

			// -----------------------------------------------------------------
			// Allow new reset from any active phase (creative sequences)
			// -----------------------------------------------------------------
			if (s.phase != Phase::IDLE && resetAcquired) {
				s.phase = Phase::AERIAL;
				s.phaseTimer = 0.f;
			}

			// -----------------------------------------------------------------
			// Final: NaN-safe, non-negative
			// -----------------------------------------------------------------
			float out = reward * scale;
			if (!std::isfinite(out)) out = 0.f;
			return std::max(0.f, out);
		}

	private:

		enum class Phase {
			IDLE,
			AERIAL,      // reset acquired, tracking ball toward backboard
			FINISHING,   // backboard hit, waiting for second touch
			GOAL_WATCH   // second touch done, brief window for goal
		};

		struct PerCarState {
			Phase phase = Phase::IDLE;
			float phaseTimer = 0.f;

			bool prevHasFlip = false;
			bool prevGotReset = false;

			float groundGrace = 0.f;

			bool prevBallNearBackboard = false;
			bool prevGoalScored = false;
		};

		// =====================================================================
		// Utility
		// =====================================================================

		static float Saturate01(float x) {
			if (x <= 0.f) return 0.f;
			if (x >= 1.f) return 1.f;
			return x;
		}

		// =====================================================================
		// Ground detection
		// =====================================================================

		bool IsGrounded(const Player& player, float dt, float& grace) const {
			const float wallDistX = CommonValues::SIDE_WALL_X - std::abs(player.pos.x);
			const float wallDistY = CommonValues::BACK_WALL_Y - std::abs(player.pos.y);
			const float wallDist = std::max(0.f, std::min(wallDistX, wallDistY));

			const bool vzOk = (std::abs(player.vel.z) <= maxAbsVzForGround);
			const bool lowZ = (player.pos.z <= groundMaxCarZ);
			const bool nearWall = (wallDist <= wallNearDist) && (player.pos.z <= wallMaxCarZ);
			const bool groundNow = vzOk && (lowZ || nearWall);

			if (groundNow) grace = groundGraceSec;
			else           grace = std::max(0.f, grace - dt);

			return (grace > 0.f);
		}

		// =====================================================================
		// Opponent goal / backboard helpers
		// =====================================================================

		Vec OppGoalCenter(const Player& player) const {
			return (player.team == RocketSim::Team::BLUE)
				? CommonValues::ORANGE_GOAL_CENTER
				: CommonValues::BLUE_GOAL_CENTER;
		}

		bool IsOppGoalScored(const Player& player, const GameState& state) const {
			return (state.ball.pos.y * OppGoalCenter(player).y) > 0.f;
		}

		bool IsBallNearOppBackboard(const Player& player, const GameState& state) const {
			const Vec oppGoal = OppGoalCenter(player);
			if (state.ball.pos.y * oppGoal.y <= 0.f) return false;

			const float distToWall = CommonValues::BACK_WALL_Y - std::abs(state.ball.pos.y);
			if (distToWall > backboardBallWallDist) return false;

			if (state.ball.pos.z < backboardMinBallZ) return false;

			return true;
		}

		// =====================================================================
		// Per-tick rewards
		// =====================================================================

		/// Height matching: full reward inside deadzone, linear falloff beyond.
		float ComputeHeightMatchReward(const Player& player, const GameState& state) const {
			const float dz = std::abs(player.pos.z - state.ball.pos.z);
			if (dz <= heightDeadzone) return heightMatchRewardPerTick;

			const float excess = dz - heightDeadzone;
			const float factor = 1.0f - Saturate01(excess / heightFalloffRange);
			return heightMatchRewardPerTick * factor;
		}

		/// X proximity (lateral): tight, simple linear falloff.
		float ComputeXProximityReward(const Player& player, const GameState& state) const {
			const float dx = std::abs(player.pos.x - state.ball.pos.x);
			const float factor = 1.0f - Saturate01(dx / xMaxDist);
			return xProximityRewardPerTick * factor;
		}

		/// Y proximity (depth along field): band / sweet-spot reward.
		///   Full reward when |dy| is in [yIdealMin, yIdealMax].
		///   Falls off toward 0 if too close or too far.
		float ComputeYProximityReward(const Player& player, const GameState& state) const {
			const float dy = std::abs(player.pos.y - state.ball.pos.y);

			// Inside sweet spot -> full reward
			if (dy >= yIdealMin && dy <= yIdealMax) {
				return yProximityRewardPerTick;
			}

			// Too close: falloff from yIdealMin down to (yIdealMin - yCloseFalloff)
			if (dy < yIdealMin) {
				const float deficit = yIdealMin - dy;
				const float factor = 1.0f - Saturate01(deficit / yCloseFalloff);
				return yProximityRewardPerTick * factor;
			}

			// Too far: falloff from yIdealMax up to (yIdealMax + yFarFalloff)
			const float excess = dy - yIdealMax;
			const float factor = 1.0f - Saturate01(excess / yFarFalloff);
			return yProximityRewardPerTick * factor;
		}

		// =====================================================================
		// Second-touch bonuses
		// =====================================================================

		float ComputeDirectionBonus(const Player& player, const GameState& state) const {
			const Vec oppGoal = OppGoalCenter(player);

			Vec goalDir;
			goalDir.x = oppGoal.x - state.ball.pos.x;
			goalDir.y = oppGoal.y - state.ball.pos.y;
			goalDir.z = oppGoal.z - state.ball.pos.z;
			const float goalLen = std::sqrt(
				goalDir.x * goalDir.x + goalDir.y * goalDir.y + goalDir.z * goalDir.z);
			if (goalLen <= 1e-3f) return directionBonusMax;
			goalDir.x /= goalLen;
			goalDir.y /= goalLen;
			goalDir.z /= goalLen;

			const Vec& vel = state.ball.vel;
			const float velLen = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
			if (velLen <= 1e-3f) return 0.f;

			const float cosAng = std::clamp(
				(vel.x * goalDir.x + vel.y * goalDir.y + vel.z * goalDir.z) / velLen,
				-1.0f, 1.0f);

			return directionBonusMax * Saturate01(cosAng);
		}

		float ComputeSpeedBonus(const GameState& state) const {
			constexpr float UU_PER_SEC_TO_KMH = 0.036f;
			const float vxy_kmh = std::sqrt(
				state.ball.vel.x * state.ball.vel.x +
				state.ball.vel.y * state.ball.vel.y) * UU_PER_SEC_TO_KMH;

			const float t = Saturate01(
				(vxy_kmh - speedBonusMinKmh) /
				std::max(1e-3f, speedBonusMaxKmh - speedBonusMinKmh));
			return speedBonusMax * t;
		}

		// =====================================================================
		// Parameters
		// =====================================================================

		float scale;
		float minAirZForReset;

		float groundMaxCarZ;
		float wallNearDist;
		float wallMaxCarZ;
		float maxAbsVzForGround;
		float groundGraceSec;

		float backboardBallWallDist;
		float backboardMinBallZ;
		float backboardHitReward;

		float heightMatchRewardPerTick;
		float heightDeadzone;
		float heightFalloffRange;

		float xProximityRewardPerTick;
		float xMaxDist;

		float yProximityRewardPerTick;
		float yIdealMin;
		float yIdealMax;
		float yCloseFalloff;
		float yFarFalloff;

		float secondTouchReward;
		float directionBonusMax;
		float speedBonusMax;
		float speedBonusMinKmh;
		float speedBonusMaxKmh;

		float goalScoredBonus;

		float maxAerialWaitSec;
		float maxFinishingWaitSec;

		std::unordered_map<uint32_t, PerCarState> states;
	};

	class VelocityBallToGoalReward : public Reward {
	public:
		bool ownGoal = false;
		VelocityBallToGoalReward(bool ownGoal = false) : ownGoal(ownGoal) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			bool targetOrangeGoal = player.team == Team::BLUE;
			if (ownGoal)
				targetOrangeGoal = !targetOrangeGoal;

			Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

			Vec ballDirToGoal = (targetPos - state.ball.pos).Normalized();
			return ballDirToGoal.Dot(state.ball.vel / CommonValues::BALL_MAX_SPEED);
		}
	};

	class EnergyReward : public Reward {
	public:
		const double GRAVITY = 650;
		const double MASS = 180;
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			auto max_energy = (MASS * GRAVITY * (CEILING_Z - 17.)) + (0.5 * MASS * (CAR_MAX_SPEED * CAR_MAX_SPEED));
			double energy = 0;
			double velocity = player.vel.Length();

			if (player.HasFlipOrJump()) {
				energy += 0.35 * MASS * (292 * 292);
			}
			if (player.HasFlipOrJump() and !player.isOnGround) {
				double dodge_impulse = (velocity <= 1700) ? (500 + (velocity / 17)) : (600 - (velocity - 1700));
				dodge_impulse = std::max(dodge_impulse - 25, 0.0);
				energy += 0.9 * 0.5 * MASS * (dodge_impulse * dodge_impulse);
				energy += 0.35 * MASS * 550. * 550.;
			}
			//height
			energy += MASS * GRAVITY * (player.pos.z - 17.) * 0.75; // fudge factor to reduce height
			//KE
			energy += 0.5 * MASS * velocity * velocity;
			//boost

			energy += 7.97e5 * std::clamp(player.boost, 0.0f, 100.0f);

			double norm_energy = player.isDemoed ? 0.0f : (energy / max_energy);
			return norm_energy;
		}
	};


	class AirReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			return !player.isOnGround;
		}
	};

	class Existence : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			return 1;
		}
	};

	class BoostSuperSonicPenalty : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround && player.isSupersonic && (player.boost < player.prev->boost)) {
				return 1;
			}
		}
	};

	// ============================================================================
	// UTILITY FUNCTIONS
	// ============================================================================

	/**
	 * @brief Calculate exponential reward based on distance
	 */
	inline float ExpReward(float current, float target, float scale = 1.0f) {
		float diff = std::abs(current - target);
		return std::exp(-diff / scale);
	}

	/**
	 * @brief Calculate linear interpolation reward
	 */
	inline float LinearReward(float current, float min, float max) {
		if (current <= min) return 0.0f;
		if (current >= max) return 1.0f;
		return (current - min) / (max - min);
	}

	/**
	 * @brief Safe normalization helper
	 */
	inline Vec SafeNormalize(const Vec& v, const Vec& fallback = Vec(0, 0, 0)) {
		float len = v.Length();
		return (len > 1e-6f) ? (v / len) : fallback;
	}

} // namespace RLGC