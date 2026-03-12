#pragma once

#include "AdvancedObs.h"
#include <RLGymCPP/CommonValues.h>
#include <RLGymCPP/Gamestates/StateUtil.h>
#include "../../../RocketSim/src/RLConst.h"
#include <cmath>

namespace RLGC {

	// Custom observation builder — non-standard feature layout, derived physics features,
	// dash timers, and unique normalizations. Do not distribute.
	class CustomObs : public AdvancedObs {
	public:
		static constexpr int MAX_TEAMMATES  = 2;
		static constexpr int MAX_OPPONENTS  = 3;

		// Base AdvancedObs per-player = 29, plus 9 custom features + 1 team flag + 5 dash timers = 44
		static constexpr int PLAYER_FEAT_SIZE = 44;

		// Dash timer normalization (match RLConst)
		static constexpr float JUMP_TIME_MAX   = 0.2f;   // JUMP_MAX_TIME
		static constexpr float FLIP_TIME_MAX   = 0.65f;   // FLIP_TORQUE_TIME
		static constexpr float AIR_TIME_MAX    = 1.25f;   // DOUBLEJUMP_MAX_DELAY
		static constexpr float BOOST_TIME_MAX  = 0.1f;    // BOOST_MIN_TIME

		// --- Custom normalization constants (non-standard, unique to this obs) ---
		static constexpr float DIST_COEF       = 1.f / 3000.f;   // field ~4096 x 5120
		static constexpr float SPEED_COEF      = 1.f / 2300.f;   // max car speed
		static constexpr float BALL_SPEED_COEF = 1.f / 6000.f;   // max ball speed (high aerial shots)
		static constexpr float TIME_COEF       = 1.f / 5.f;      // time estimates in seconds

		// Goal positions (blue scores into orange goal at +y)
		static constexpr float GOAL_Y    = 5120.f;
		static constexpr float GOAL_Z    =  321.f;

		// -----------------------------------------------------------------------
		// Helper: goal positions from perspective of player team
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

		void AddCustomPlayerToObs(
			FList& out,
			const Player& player,
			bool inv,
			const PhysState& ball,
			bool isClosestToBall,
			bool isClosestOnTeam,
			const Vec& ownGoal,
			const Vec& enemyGoal,
			float teamFlag
		) {
			AddPlayerToObs(out, player, inv, ball);

			float distBall = (ball.pos - player.pos).Length();
			out += distBall * DIST_COEF;

			float carSpeed = player.vel.Length();
			out += carSpeed * SPEED_COEF;

			float distOwnGoal = (ownGoal - player.pos).Length();
			out += distOwnGoal * DIST_COEF;

			float distEnemyGoal = (enemyGoal - player.pos).Length();
			out += distEnemyGoal * DIST_COEF;

			float ttb = TimeToBall(player, ball);
			out += std::min(ttb * TIME_COEF, 1.f);

			out += isClosestToBall ? 1.f : 0.f;
			out += isClosestOnTeam ? 1.f : 0.f;

			Vec toBall = (ball.pos - player.pos);
			float toBallLen = toBall.Length();
			float alignment = 0.f;
			if (toBallLen > 1.f) {
				Vec toBallNorm = toBall * (1.f / toBallLen);
				alignment = player.rotMat.forward.Dot(toBallNorm);
			}
			out += alignment;

			out += BallGoalProgress(ball, inv);

			out += std::min(player.jumpTime / JUMP_TIME_MAX, 1.f);
			out += std::min(player.flipTime / FLIP_TIME_MAX, 1.f);
			out += std::min(player.airTimeSinceJump / AIR_TIME_MAX, 1.f);
			out += std::min(player.timeSpentBoosting / BOOST_TIME_MAX, 1.f);
			out += std::min(std::max(player.handbrakeVal, 0.f), 1.f);

			out += teamFlag;
		}

		virtual FList BuildObs(const Player& player, const GameState& state) override {
			FList obs = {};

			bool inv = (player.team == Team::ORANGE);

			auto ball    = InvertPhys(state.ball, inv);
			auto& pads      = state.GetBoostPads(inv);
			auto& padTimers = state.GetBoostPadTimers(inv);

			Vec ownGoal   = OwnGoalPos(inv);
			Vec enemyGoal = EnemyGoalPos(inv);

			obs += ball.pos * POS_COEF;
			obs += ball.vel  / CommonValues::BALL_MAX_SPEED;
			obs += ball.angVel * ANG_VEL_COEF;
			obs += ball.vel.Length() * BALL_SPEED_COEF;

			Vec ballToOwn   = (ownGoal   - ball.pos);
			Vec ballToEnemy = (enemyGoal - ball.pos);
			obs += ballToOwn   * DIST_COEF;
			obs += ballToEnemy * DIST_COEF;
			obs += BallGoalProgress(ball, inv);

			for (int i = 0; i < player.prevAction.ELEM_AMOUNT; i++)
				obs += player.prevAction[i];

			for (int i = 0; i < CommonValues::BOOST_LOCATIONS_AMOUNT; i++) {
				if (pads[i]) {
					obs += 1.f;
				} else {
					obs += std::exp(-padTimers[i] * 0.2f);
				}
			}

			int   closestGlobalId  = -1;
			int   closestBlueId    = -1;
			int   closestOrangeId  = -1;
			float closestGlobalDist = 1e9f;
			float closestBlueDist   = 1e9f;
			float closestOrangeDist = 1e9f;

			for (auto& p : state.players) {
				if (p.isDemoed) continue;
				float d = (state.ball.pos - p.pos).Length();
				if (d < closestGlobalDist) {
					closestGlobalDist = d;
					closestGlobalId   = p.carId;
				}
				if (p.team == Team::BLUE && d < closestBlueDist) {
					closestBlueDist = d;
					closestBlueId   = p.carId;
				}
				if (p.team == Team::ORANGE && d < closestOrangeDist) {
					closestOrangeDist = d;
					closestOrangeId   = p.carId;
				}
			}

			bool selfClosestGlobal = (player.carId == closestGlobalId);
			bool selfClosestTeam   = (player.carId == (inv ? closestOrangeId : closestBlueId));
			AddCustomPlayerToObs(obs, player, inv, ball, selfClosestGlobal, selfClosestTeam, ownGoal, enemyGoal, 0.f);

			FList teammates = {}, opponents = {};

			for (auto& other : state.players) {
				if (other.carId == player.carId) continue;

				bool isTeammate = (other.team == player.team);
				bool otherClosestGlobal = (other.carId == closestGlobalId);
				bool otherClosestTeam;
				if (isTeammate)
					otherClosestTeam = (other.carId == (inv ? closestOrangeId : closestBlueId));
				else
					otherClosestTeam = (other.carId == (inv ? closestBlueId   : closestOrangeId));

				FList& target = isTeammate ? teammates : opponents;
				float teamFlag = isTeammate ? 1.f : -1.f;
				AddCustomPlayerToObs(target, other, inv, ball, otherClosestGlobal, otherClosestTeam, ownGoal, enemyGoal, teamFlag);
			}

			auto padZeros = [&](FList& out, int missing) {
				int elems = missing * PLAYER_FEAT_SIZE;
				for (int i = 0; i < elems; i++) out += 0.f;
			};

			int curTM = (int)(teammates.size() / PLAYER_FEAT_SIZE);
			int missTM = std::max(0, MAX_TEAMMATES - curTM);
			obs += teammates;
			padZeros(obs, missTM);

			int curOpp = (int)(opponents.size() / PLAYER_FEAT_SIZE);
			int missOpp = std::max(0, MAX_OPPONENTS - curOpp);
			obs += opponents;
			padZeros(obs, missOpp);

			return obs;
		}
	};
}
