#pragma once

#include "AdvancedObs.h"
#include <RLGymCPP/CommonValues.h>
#include <RLGymCPP/Gamestates/StateUtil.h>
#include "../../../RocketSim/src/RLConst.h"
#include <unordered_map>
#include <cmath>

namespace RLGC {

	// Custom observation builder — non-standard feature layout, derived physics features,
	// dash timers, reset chain counter, and unique normalizations. Do not distribute.
	class CustomObs : public AdvancedObs {
	public:
		static constexpr int MAX_TEAMMATES  = 2;
		static constexpr int MAX_OPPONENTS  = 3;

		// Base AdvancedObs per-player = 29
		// + 9 custom features
		// + 5 dash timers
		// + 1 team flag
		// + 1 flip reset chain count
		// = 45
		static constexpr int PLAYER_FEAT_SIZE = 45;

		// Dash timer normalization (match RLConst)
		static constexpr float JUMP_TIME_MAX   = 0.2f;    // JUMP_MAX_TIME
		static constexpr float FLIP_TIME_MAX   = 0.65f;   // FLIP_TORQUE_TIME
		static constexpr float AIR_TIME_MAX    = 1.25f;   // DOUBLEJUMP_MAX_DELAY
		static constexpr float BOOST_TIME_MAX  = 0.1f;    // BOOST_MIN_TIME

		// Reset chain normalization
		static constexpr float RESET_CHAIN_MAX = 8.f;

		// --- Custom normalization constants ---
		static constexpr float DIST_COEF       = 1.f / 3000.f;
		static constexpr float SPEED_COEF      = 1.f / 2300.f;
		static constexpr float BALL_SPEED_COEF = 1.f / 6000.f;
		static constexpr float TIME_COEF       = 1.f / 5.f;

		// Goal positions
		static constexpr float GOAL_Y = 5120.f;
		static constexpr float GOAL_Z =  321.f;

		// -----------------------------------------------------------------------
		// Per-player tracked state
		// -----------------------------------------------------------------------
		struct PlayerTrackedState {
			int resetChain = 0; // current preflip reset chain count
		};

		std::unordered_map<uint32_t, PlayerTrackedState> trackedStates;

		// Call from your reward when a preflip reset is confirmed
		void OnPreFlipReset(uint32_t carId) {
			trackedStates[carId].resetChain++;
		}

		// Call from your reward when a player lands
		void OnPlayerLanded(uint32_t carId) {
			trackedStates[carId].resetChain = 0;
		}

		virtual void Reset(const GameState& state) override {
			trackedStates.clear();
			for (auto& player : state.players)
				trackedStates[player.carId] = {};
		}

		// -----------------------------------------------------------------------
		// Helpers
		// -----------------------------------------------------------------------
		static Vec OwnGoalPos(bool inv) {
			return Vec(0.f, inv ? GOAL_Y : -GOAL_Y, GOAL_Z);
		}

		static Vec EnemyGoalPos(bool inv) {
			return Vec(0.f, inv ? -GOAL_Y : GOAL_Y, GOAL_Z);
		}

		static float TimeToBall(const PhysState& car, const PhysState& ball) {
			float dist = (ball.pos - car.pos).Length();
			float spd  = car.vel.Length();
			return dist / (spd + 500.f);
		}

		static float BallGoalProgress(const PhysState& ball, bool inv) {
			float dir = inv ? -1.f : 1.f;
			return (ball.vel.y * dir) * BALL_SPEED_COEF;
		}

		// -----------------------------------------------------------------------
		// Per-player obs: 29 (base) + 9 (custom) + 5 (dash timers) + 1 (team flag)
		//               + 1 (reset chain) = 45
		// NOTE: player and ball must already be in the correct inverted frame
		// -----------------------------------------------------------------------
		void AddCustomPlayerToObs(
			FList& out,
			const Player& player,
			const PhysState& invertedPhys,
			bool inv,
			const PhysState& ball,
			bool isClosestToBall,
			bool isClosestOnTeam,
			const Vec& ownGoal,
			const Vec& enemyGoal,
			float teamFlag,
			const PlayerTrackedState& tracked
		) {
			// --- 29 standard AdvancedObs features ---
			AddPlayerToObs(out, player, inv, ball);

			// --- 9 custom derived features ---

			// 1. Normalised distance to ball
			out += (ball.pos - invertedPhys.pos).Length() * DIST_COEF;

			// 2. Normalised car speed
			out += invertedPhys.vel.Length() * SPEED_COEF;

			// 3. Normalised distance to own goal
			out += (ownGoal - invertedPhys.pos).Length() * DIST_COEF;

			// 4. Normalised distance to enemy goal
			out += (enemyGoal - invertedPhys.pos).Length() * DIST_COEF;

			// 5. Time-to-ball estimate (clamped 0-1)
			out += std::min(TimeToBall(invertedPhys, ball) * TIME_COEF, 1.f);

			// 6. Is globally closest to ball?
			out += isClosestToBall ? 1.f : 0.f;

			// 7. Is closest on team?
			out += isClosestOnTeam ? 1.f : 0.f;

			// 8. Alignment toward ball
			Vec toBall = (ball.pos - invertedPhys.pos);
			float toBallLen = toBall.Length();
			float alignment = 0.f;
			if (toBallLen > 1.f)
				alignment = invertedPhys.rotMat.forward.Dot(toBall * (1.f / toBallLen));
			out += alignment;

			// 9. Ball progress toward enemy goal
			out += BallGoalProgress(ball, inv);

			// --- 5 dash timers (normalised 0-1) ---
			out += std::min(player.jumpTime          / JUMP_TIME_MAX,  1.f);
			out += std::min(player.flipTime          / FLIP_TIME_MAX,  1.f);
			out += std::min(player.airTimeSinceJump  / AIR_TIME_MAX,   1.f);
			out += std::min(player.timeSpentBoosting / BOOST_TIME_MAX, 1.f);
			out += std::min(std::max(player.handbrakeVal, 0.f),        1.f);

			// --- 1 team flag ---
			out += teamFlag;

			// --- 1 flip reset chain count (normalised, clamped) ---
			out += std::min((float)tracked.resetChain / RESET_CHAIN_MAX, 1.f);
		}

		// -----------------------------------------------------------------------
		// Main obs builder
		// -----------------------------------------------------------------------
		virtual FList BuildObs(const Player& player, const GameState& state) override {
			FList obs = {};

			bool inv = (player.team == Team::ORANGE);

			auto ball       = InvertPhys(state.ball, inv);
			auto& pads      = state.GetBoostPads(inv);
			auto& padTimers = state.GetBoostPadTimers(inv);

			Vec ownGoal   = OwnGoalPos(inv);
			Vec enemyGoal = EnemyGoalPos(inv);

			// ----------------------------------------------------------------
			// --- BALL ---
			// ----------------------------------------------------------------
			obs += ball.pos * POS_COEF;
			obs += ball.vel / CommonValues::BALL_MAX_SPEED;
			obs += ball.angVel * ANG_VEL_COEF;

			obs += ball.vel.Length() * BALL_SPEED_COEF;

			obs += (ownGoal   - ball.pos) * DIST_COEF;
			obs += (enemyGoal - ball.pos) * DIST_COEF;

			obs += BallGoalProgress(ball, inv);

			// ----------------------------------------------------------------
			// --- PREVIOUS ACTION ---
			// ----------------------------------------------------------------
			for (int i = 0; i < player.prevAction.ELEM_AMOUNT; i++)
				obs += player.prevAction[i];

			// ----------------------------------------------------------------
			// --- BOOST PADS ---
			// ----------------------------------------------------------------
			for (int i = 0; i < CommonValues::BOOST_LOCATIONS_AMOUNT; i++) {
				if (pads[i])
					obs += 1.f;
				else
					obs += std::exp(-padTimers[i] * 0.2f);
			}

			// ----------------------------------------------------------------
			// --- Closest to ball precompute ---
			// ----------------------------------------------------------------
			int   closestGlobalId   = -1;
			int   closestBlueId     = -1;
			int   closestOrangeId   = -1;
			float closestGlobalDist = 1e9f;
			float closestBlueDist   = 1e9f;
			float closestOrangeDist = 1e9f;

			for (auto& p : state.players) {
				if (p.isDemoed) continue;
				float d = (state.ball.pos - p.pos).Length();
				if (d < closestGlobalDist) { closestGlobalDist = d; closestGlobalId  = p.carId; }
				if (p.team == Team::BLUE   && d < closestBlueDist)  { closestBlueDist   = d; closestBlueId   = p.carId; }
				if (p.team == Team::ORANGE && d < closestOrangeDist) { closestOrangeDist = d; closestOrangeId = p.carId; }
			}

			// ----------------------------------------------------------------
			// --- SELF ---
			// ----------------------------------------------------------------
			auto selfPhys          = InvertPhys(player, inv);
			bool selfClosestGlobal = (player.carId == closestGlobalId);
			bool selfClosestTeam   = (player.carId == (inv ? closestOrangeId : closestBlueId));
			auto& selfTracked      = trackedStates[player.carId];

			AddCustomPlayerToObs(obs, player, selfPhys, inv, ball, selfClosestGlobal, selfClosestTeam, ownGoal, enemyGoal, 0.f, selfTracked);

			// ----------------------------------------------------------------
			// --- TEAMMATES + OPPONENTS ---
			// BUG FIX: now correctly passes inverted phys for each other player
			// ----------------------------------------------------------------
			FList teammates = {}, opponents = {};

			for (auto& other : state.players) {
				if (other.carId == player.carId) continue;

				bool isTeammate = (other.team == player.team);

				// FIX: invert the other player into our frame before building obs
				auto otherPhys = InvertPhys(other, inv);

				bool otherClosestGlobal = (other.carId == closestGlobalId);
				bool otherClosestTeam   = isTeammate
					? (other.carId == (inv ? closestOrangeId : closestBlueId))
					: (other.carId == (inv ? closestBlueId   : closestOrangeId));

				FList& target   = isTeammate ? teammates : opponents;
				float  teamFlag = isTeammate ? 1.f : -1.f;
				auto&  otherTracked = trackedStates[other.carId];

				AddCustomPlayerToObs(target, other, otherPhys, inv, ball, otherClosestGlobal, otherClosestTeam, ownGoal, enemyGoal, teamFlag, otherTracked);
			}

			// ----------------------------------------------------------------
			// --- PAD to MAX ---
			// ----------------------------------------------------------------
			auto padZeros = [&](FList& out, int missing) {
				for (int i = 0; i < missing * PLAYER_FEAT_SIZE; i++) out += 0.f;
			};

			obs += teammates;
			padZeros(obs, std::max(0, MAX_TEAMMATES - (int)(teammates.size() / PLAYER_FEAT_SIZE)));

			obs += opponents;
			padZeros(obs, std::max(0, MAX_OPPONENTS - (int)(opponents.size() / PLAYER_FEAT_SIZE)));

			return obs;
		}
	};
}
