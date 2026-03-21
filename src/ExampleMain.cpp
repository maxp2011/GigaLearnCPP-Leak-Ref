#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObs.h>
#include <RLGymCPP/ObsBuilders/AdvancedObs.h>
#include <RLGymCPP/ObsBuilders/AdvancedObsPadded.h>
#include <RLGymCPP/ObsBuilders/CustomObs.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/FuzzedKickoffState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>

namespace fs = std::filesystem;

// Set from main() via --players-per-team (1 = 1v1, 2 = 2v2, 3 = 3v3). Must match how you run in RLBot.
static int g_playersPerTeam = 1;

// Find collision_meshes folder: try cwd, then parent dirs. Prefer absolute path so it works from build/.
static std::string FindCollisionMeshesPath() {
	const char* candidates[] = { "collision_meshes", "../collision_meshes", "../../collision_meshes" };
	for (const char* sub : candidates) {
		fs::path p = fs::current_path() / sub;
		fs::path soccar = p / "soccar";
		if (fs::exists(p) && fs::is_directory(p) && fs::exists(soccar) && fs::is_directory(soccar)) {
			try {
				return fs::absolute(p).string();
			} catch (...) {}
			return p.string();
		}
	}
	return "collision_meshes"; // fallback (Learner will init; may fail if missing)
}
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

using namespace GGL;
using namespace RLGC;

static bool ParseBoolArg(int argc, char* argv[], const char* flag, bool defaultValue) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], flag) == 0) return true;
		std::string arg = argv[i];
		if (arg == std::string("--no-") + (flag + 2)) return false;
	}
	return defaultValue;
}
static int ParseIntArg(int argc, char* argv[], const char* flag, int defaultValue) {
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], flag) == 0) {
			int v = std::atoi(argv[i + 1]);
			return v > 0 ? v : defaultValue;
		}
	}
	return defaultValue;
}
static float ParseFloatArg(int argc, char* argv[], const char* flag, float defaultValue) {
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], flag) == 0)
			return (float)std::atof(argv[i + 1]);
	}
	return defaultValue;
}
static std::string ParseStrArg(int argc, char* argv[], const char* flag, const char* defaultValue) {
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], flag) == 0)
			return argv[i + 1];
	}
	return defaultValue ? std::string(defaultValue) : std::string();
}

// Create the RLGymCPP environment for each of our games (rewards/weights/hyperparams from your config)
EnvCreateResult EnvCreateFunc(int index) {
	std::vector<WeightedReward> rewards = {
		// ===================== CORE OBJECTIVE =====================
		{ new RLGC::GoalReward(1.0f, -1.0f),                           500.0f },
		{ new RLGC::ConcedeDistancePenalty(),                           350.0f },

		// ===================== BALL -> GOAL SHAPING (FAST PLAY) ===
		{ new ZeroSumReward(new RLGC::VelocityBallToGoalReward(), 0.0f, 1.0f),  16.0f },
		{ new RLGC::VelocityPlayerToBallReward(),                       0.20f },

		// ===================== TOUCH QUALITY ======================
		{ new ZeroSumReward(new RLGC::TouchBallReward(), 0.0f, 1.0f),   1.0f },
		{ new RLGC::TouchAccelReward(),                                 5.0f },
		{ new RLGC::StrongTouchReward(),                                1.0f },

		// ===================== AERIAL / ADVANCED ==================
		{ new ZeroSumReward(new RLGC::FlyToGoalKeepHigh(), 0.0f, 0.01f), 0.2f },
		{ new RLGC::FlipResetMegaRewardSimple(),                        0.01f },
		{ new RLGC::PopResetReward(),                                   55.0f },
		{ new RLGC::PopResetDoubleTapReward(),                          2.0f },
		{ new RLGC::MustyFlickAfterResetGoalReward(),                   1.0f },

		// ===================== MOVEMENT / MECHANICS (LIGHT) ========
		{ new RLGC::HyperNoStackReward(),                               0.25f },

		// ===================== PHYSICAL (LOW, NON-CORE) =============
		{ new ZeroSumReward(new RLGC::BumpReward(), 0.0f, 1.0f),        2.0f },
		{ new ZeroSumReward(new RLGC::DemoReward(), 0.0f, 1.0f),        10.0f },

		// ===================== BOOST ECONOMY ========================
		{ new ZeroSumReward(new RLGC::PickupBoostReward(), 0.0f, 1.0f), 0.5f },
		{ new RLGC::SaveBoostReward(),                                  0.37f },

		// ===================== KICKOFF ==============================
		{ new ZeroSumReward(new RLGC::KickoffFirstTouchReward(), 0.0f, 1.0f), 80.0f },

		// ===================== REGULARIZATION =======================
		{ new RLGC::EnergyReward(),                                     0.05f },
	};

	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(30),
		new GoalScoreCondition()
	};

	GameMode gameMode = GameMode::SOCCAR;

	auto arena = Arena::Create(gameMode);
	for (int i = 0; i < g_playersPerTeam; i++) {
		arena->AddCar(Team::BLUE);
		arena->AddCar(Team::ORANGE);
	}

	EnvCreateResult result = {};
	result.actionParser = new DefaultAction();
	result.obsBuilder = new CustomObs();  // Student uses CustomObs; TL teacher uses AdvancedObs
	result.stateSetter = new CombinedState(
		std::vector<std::pair<StateSetter*, float>>{
			{ new FuzzedKickoffState(), 0.5f },
			{ new RandomState(true, true, true), 0.5f },
		}
	);
	result.terminalConditions = terminalConditions;
	result.rewards = rewards;
	result.arena = arena;

	return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
	bool doExpensiveMetrics = (rand() % 8) == 0;
	const int maxPlayers = 2000;
	int count = 0;
	bool done = false;

	for (auto& state : states) {
		if (done) break;
		for (auto& player : state.players) {
			if (count++ >= maxPlayers) { done = true; break; }
			report.AddAvg("Player/In Air Ratio", !player.isOnGround);
			report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
			report.AddAvg("Player/Demoed Ratio", player.isDemoed);
			report.AddAvg("Player/Boost", player.boost);

			bool hasFlipReset = player.HasFlipReset();
			bool gotFlipReset = player.GotFlipReset();
			bool hasFlipOrJump = player.HasFlipOrJump();
			report.AddAvg("Player/Has Flip Reset Ratio", hasFlipReset);
			report.AddAvg("Player/Got Flip Reset Ratio", gotFlipReset);
			report.AddAvg("Player/Has Flip Or Jump Ratio", hasFlipOrJump);
			report.AddAvg("Player/Is Flipping Ratio", player.isFlipping);
			if (player.ballTouchedStep && !player.isOnGround)
				report.AddAvg("Player/Aerial Touch Height", state.ball.pos.z);

			report.AddAvg("Player/Goal Ratio", player.eventState.goal);
			report.AddAvg("Player/Assist Ratio", player.eventState.assist);
			report.AddAvg("Player/Shot Ratio", player.eventState.shot);
			report.AddAvg("Player/Save Ratio", player.eventState.save);
			report.AddAvg("Player/Bump Ratio", player.eventState.bump);
			report.AddAvg("Player/Bumped Ratio", player.eventState.bumped);
			report.AddAvg("Player/Demo Ratio", player.eventState.demo);

			if (doExpensiveMetrics) {
				report.AddAvg("Player/Speed", player.vel.Length());
				Vec dirToBall = (state.ball.pos - player.pos).Normalized();
				report.AddAvg("Player/Speed Towards Ball", RS_MAX(0.f, player.vel.Dot(dirToBall)));
				if (player.ballTouchedStep)
					report.AddAvg("Player/Touch Height", state.ball.pos.z);
			}
		}

		if (state.goalScored)
			report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
	}
}

int main(int argc, char* argv[]) {
	// Initialize RocketSim with collision meshes (auto-find: cwd, ../, ../../ so server and PC both work)
	std::string meshPath = FindCollisionMeshesPath();
	RocketSim::Init(meshPath);

	// Make configuration for the learner
	LearnerConfig cfg = {};

	// Default: load/save from local checkpoints (GigaLearnCPP-Leak-Ref\build\Release\checkpoints). Use --checkpoint <path> to override, --no-load to start fresh.
	std::string checkpointPath = ParseStrArg(argc, argv, "--checkpoint", "");
	bool skipLoad = ParseBoolArg(argc, argv, "--no-load", false);
	if (skipLoad)
		cfg.checkpointFolder = "checkpoints";
	else
		cfg.checkpointFolder = checkpointPath.empty() ? "checkpoints" : checkpointPath;

	// Default: GPU (training server). Use --cpu for CPU-only (e.g. unsupported GPU / RTX 50 prebuilt torch).
	bool useCpu = ParseBoolArg(argc, argv, "--cpu", false);
	bool useGpu = ParseBoolArg(argc, argv, "--gpu", false);
	cfg.deviceType = (useCpu && !useGpu) ? LearnerDeviceType::CPU : LearnerDeviceType::GPU_CUDA;

	// Self-play vs past checkpoints: ~2x policy forward on many steps when triggered (very heavy on CPU).
	if (ParseBoolArg(argc, argv, "--no-old-versions", false))
		cfg.trainAgainstOldVersions = false;
	else
		cfg.trainAgainstOldVersions = true;
	cfg.trainAgainstOldChance = 0.4f;
	cfg.tickSkip = 8;
	cfg.actionDelay = cfg.tickSkip - 1;

	cfg.numGames = 4096;
	cfg.randomSeed = -1;

	cfg.ppo.useHalfPrecision = true;

	int tsPerItr = 1'000'000;
	cfg.ppo.tsPerItr = tsPerItr;
	cfg.ppo.batchSize = tsPerItr;
	cfg.ppo.miniBatchSize = 25'000;
	cfg.ppo.epochs = 1;
	cfg.tsPerSave = 20'000'000;

	// Normalized entropy scale; gaeGamma — tune toward ~0.9966 if desired
	cfg.ppo.entropyScale = 0.035f;
	cfg.ppo.maskEntropy = false;
	cfg.ppo.gaeGamma = 0.9955f;
	cfg.ppo.gaeLambda = 0.95f;

	cfg.ppo.policyLR = 12e-5f;
	cfg.ppo.criticLR = 12e-5f;

	cfg.ppo.sharedHead.layerSizes = { 2048, 2048 };
	cfg.ppo.policy.layerSizes = { 1024, 512, 512 };
	cfg.ppo.critic.layerSizes = { 1024, 512, 512 };

	cfg.checkpointsToKeep = 4000;

	auto optim = ModelOptimType::ADAM;  // rocket-learn / SB3 default
	cfg.ppo.policy.optimType = optim;
	cfg.ppo.critic.optimType = optim;
	cfg.ppo.sharedHead.optimType = optim;

	//auto activation = ModelActivationType::RELU;
	auto activation = ModelActivationType::LEAKY_RELU;
	cfg.ppo.policy.activationType = activation;
	cfg.ppo.critic.activationType = activation;
	cfg.ppo.sharedHead.activationType = activation;

	bool addLayerNorm = true;
	cfg.ppo.policy.addLayerNorm = addLayerNorm;
	cfg.ppo.critic.addLayerNorm = addLayerNorm;
	cfg.ppo.sharedHead.addLayerNorm = addLayerNorm;

	cfg.skillTracker.enabled = true;
	cfg.skillTracker.numArenas = 16;
	cfg.skillTracker.simTime = 45.f;
	cfg.skillTracker.updateInterval = 14;  // Less frequent = more SPS

	cfg.addRewardsToMetrics = ParseBoolArg(argc, argv, "--add-rewards", true);

	int numGamesOverride = ParseIntArg(argc, argv, "--num-games", 0);
	if (numGamesOverride > 0)
		cfg.numGames = numGamesOverride;

	if (cfg.deviceType == LearnerDeviceType::CPU) {
		std::cerr
			<< "\n*** CPU device: LibTorch on CPU is orders of magnitude slower than GPU for this model.\n"
			<< "    CustomObs (~329 floats) also costs ~3x more matmul work in layer 1 than AdvancedObs (~109).\n"
			<< "    Drop --cpu if CUDA LibTorch is available on this machine.\n";
		if (cfg.numGames > 512)
			std::cerr << "    Tip: --num-games 256 or 512 on CPU; add --no-old-versions to skip 2x inference (self-play).\n";
		else if (cfg.trainAgainstOldVersions)
			std::cerr << "    Tip: add --no-old-versions to disable self-play vs old policies (saves ~2x infer when active).\n";
		std::cerr << std::endl;
	}

	// Disable metrics by default on server (avoids python_scripts / wandb issues).
	// You can re-enable with --send-metrics true if python_scripts is set up.
	cfg.sendMetrics = ParseBoolArg(argc, argv, "--send-metrics", false);
	cfg.renderMode = ParseBoolArg(argc, argv, "--render", false);
	cfg.renderTimeScale = ParseFloatArg(argc, argv, "--render-timescale", 8.0f);

	// TL path (parse before arena): teacher kairon uses AdvancedObs @ 109 floats per player (1v1 only).
	// 2v2/3v3 changes AdvancedObs length and breaks reshape in StartTransferLearn.
	std::string tlPath = ParseStrArg(argc, argv, "--tl", "");
	if (tlPath.empty())
		tlPath = ParseStrArg(argc, argv, "--transfer-learn", "");
	if (tlPath.empty()) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--tl") == 0 || strcmp(argv[i], "--transfer-learn") == 0) {
				tlPath = "kaironTL";
				break;
			}
		}
	}

	// Arena size: must match RLBot mode (1v1 / 2v2 / 3v3). Default 1.
	{
		int ppt = ParseIntArg(argc, argv, "--players-per-team", 1);
		if (ppt < 1) ppt = 1;
		if (ppt > 3) ppt = 3;
		g_playersPerTeam = ppt;
		if (!tlPath.empty() && g_playersPerTeam != 1) {
			std::cerr << "Transfer learning: forcing --players-per-team 1 (teacher AdvancedObs is fixed 109-dim / 1v1).\n";
			std::cerr << "  After TL, train 2v2 without --tl or use a 2v2 teacher with matching obs.\n";
			g_playersPerTeam = 1;
		}
		std::cerr << "Arena: " << g_playersPerTeam << " player(s) per team (" << g_playersPerTeam << "v" << g_playersPerTeam << ").\n";
	}

	Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);

	if (!tlPath.empty()) {
		// Transfer learn FROM the new model (teacher = same arch as current: AdvancedObs, 2048x2 shared, 1024/512/512, LEAKY_RELU)
		TransferLearnConfig tlConfig = {};
		tlConfig.makeOldObsFn = []() { return new AdvancedObs(); };
		tlConfig.makeOldActFn = []() { return new DefaultAction(); };
		tlConfig.oldSharedHeadConfig.layerSizes = { 2048, 2048 };
		tlConfig.oldSharedHeadConfig.activationType = ModelActivationType::LEAKY_RELU;
		tlConfig.oldSharedHeadConfig.addLayerNorm = true;
		tlConfig.oldSharedHeadConfig.addOutputLayer = false;
		tlConfig.oldPolicyConfig.layerSizes = { 1024, 512, 512 };
		tlConfig.oldPolicyConfig.activationType = ModelActivationType::LEAKY_RELU;
		tlConfig.oldPolicyConfig.addLayerNorm = true;
		tlConfig.oldModelsPath = tlPath;
		tlConfig.lr = 4e-4f;
		tlConfig.batchSize = 32768;
		tlConfig.epochs = 5;
		tlConfig.useKLDiv = true;
		tlConfig.lossScale = 500.f;

		// Resolve to latest checkpoint if path is a folder with numbered subdirs
		if (std::filesystem::exists(tlPath) && std::filesystem::is_directory(tlPath)) {
			int64_t highest = -1;
			for (auto& entry : std::filesystem::directory_iterator(tlPath)) {
				if (!entry.is_directory()) continue;
				std::string name = entry.path().filename().string();
				bool allDigits = true;
				for (char c : name) { if (!isdigit(c)) { allDigits = false; break; } }
				if (allDigits && !name.empty()) {
					int64_t n = std::stoll(name);
					highest = std::max(highest, n);
				}
			}
			if (highest >= 0) {
				std::filesystem::path loadFolder = std::filesystem::path(tlPath) / std::to_string(highest);
				std::filesystem::path nestedFolder = loadFolder / std::to_string(highest);
				if (std::filesystem::exists(nestedFolder / "POLICY.lt") && !std::filesystem::exists(loadFolder / "POLICY.lt"))
					loadFolder = nestedFolder;
				tlConfig.oldModelsPath = loadFolder;
			}
		}

		learner->StartTransferLearn(tlConfig);
	} else {
		try {
			learner->Start();
		} catch (const std::exception& e) {
			std::string msg = e.what();
			const bool noKernelImage = (msg.find("no kernel image") != std::string::npos) ||
				(msg.find("cudaErrorNoKernelImageForDevice") != std::string::npos);
			if (cfg.deviceType == LearnerDeviceType::GPU_CUDA && noKernelImage) {
				std::cerr << "GPU not supported by this LibTorch build (no kernel image for this device)." << std::endl;
				std::cerr << "  Run with --cpu to use CPU:  ./build/GigaLearnBot --cpu" << std::endl;
				std::cerr << "  For RTX 50 / Blackwell: build LibTorch from source with the correct TORCH_CUDA_ARCH_LIST." << std::endl;
				delete learner;
				return EXIT_FAILURE;
			}
			throw;
		}
	}

	return EXIT_SUCCESS;
}

