class BSInputEnableManager {
public:
	enum {
		kUserEvent_Movement = 1 << 0,
		kUserEvent_Looking = 1 << 1,
		kUserEvent_Activate = 1 << 2,
		kUserEvent_Menu = 1 << 3,
		kUserEvent_Console = 1 << 4,
		kUserEvent_POVChange = 1 << 5,
		kUserEvent_Fighting = 1 << 6,
		kUserEvent_Sneaking = 1 << 7,
		kUserEvent_MainFourMenu = 1 << 8,
		kUserEvent_WheelZoom = 1 << 9,
		kUserEvent_Jumping = 1 << 10
	};

	enum {
		kOtherEvent_JournalTabs = 1 << 0,
		kOtherEvent_Activation = 1 << 1,
		kOtherEvent_FastTravel = 1 << 2,
		kOtherEvent_POVChange = 1 << 3,
		kOtherEvent_VATS = 1 << 4,
		kOtherEvent_FAVORITES = 1 << 5,
		kOtherEvent_PipboyLight = 1 << 6,
		kOtherEvent_ZKey = 1 << 7,
		kOtherEvent_Running = 1 << 8
	};

	struct InputEnableLayerState {
		std::uint32_t index;
		std::uint32_t state;
	};

	static BSInputEnableManager* GetSingleton() {
		REL::Relocation<BSInputEnableManager**> InputEnableManager{ REL::ID(781703) };
		auto ptr = InputEnableManager.get();
		if (ptr) return *ptr;
		return nullptr;
	}

	bool EnableUserEvent(std::uint32_t layerIdx, std::uint32_t flag, bool enable, std::uint32_t arg4) {
		using func_t = bool(*)(BSInputEnableManager*, std::uint32_t, std::uint32_t, bool, std::uint32_t);
		REL::Relocation<func_t> func{ REL::ID(1432984) };
		return func(this, layerIdx, flag, enable, arg4);
	}

	bool EnableOtherEvent(std::uint32_t layerIdx, std::uint32_t flag, bool enable, std::uint32_t arg4) {
		using func_t = bool(*)(BSInputEnableManager*, std::uint32_t, std::uint32_t, bool, std::uint32_t);
		REL::Relocation<func_t> func{ REL::ID(1419268) };
		return func(this, layerIdx, flag, enable, arg4);
	}

	void ResetInputEnableLayer(std::uint32_t layerIdx) {
		std::uint64_t flag = static_cast<std::uint64_t>(-1);
		EnableUserEvent(layerIdx, flag & 0xFFFFFFFF, true, 3);
		EnableOtherEvent(layerIdx, flag >> 32, true, 3);
	}

	std::uint64_t unk00[0x118 >> 3];
	std::uint64_t currentInputEnableMask;					// 118
	std::uint64_t unk120;
	RE::BSSpinLock inputEnableArrLock;						// 128
	RE::BSTArray<std::uint64_t>	inputEnableMaskArr;			// 130
	RE::BSTArray<InputEnableLayerState*> layerStateArr;		// 148
	RE::BSTArray<RE::BSFixedString> layerNameArr;			// 160
};

namespace ControlMap {
	bool PopInputContext(RE::ControlMap* a_controlMap, RE::UserEvents::INPUT_CONTEXT_ID a_id) {
		using func_t = decltype(&PopInputContext);
		REL::Relocation<func_t> func{ REL::ID(74587) };
		return func(a_controlMap, a_id);
	}
}

void ReleaseInputEnableLayer() {
	BSInputEnableManager* g_inputEnableManager = BSInputEnableManager::GetSingleton();
	if (!g_inputEnableManager)
		return;

	g_inputEnableManager->inputEnableArrLock.lock();

	std::uint32_t layerIndex = 0;
	for (const auto layerName : g_inputEnableManager->layerNameArr) {
		if (layerName == "Looks Menu Input Layer")
			break;
		layerIndex++;
	}

	if (layerIndex >= g_inputEnableManager->layerNameArr.size()) {
		g_inputEnableManager->inputEnableArrLock.unlock();
		return;
	}

	g_inputEnableManager->ResetInputEnableLayer(layerIndex);
	g_inputEnableManager->layerStateArr[layerIndex]->state = 1;
	g_inputEnableManager->layerNameArr[layerIndex] = "";

	g_inputEnableManager->inputEnableArrLock.unlock();
}

void ReleaseControlMap() {
	RE::ControlMap* g_controlMap = RE::ControlMap::GetSingleton();
	if (!g_controlMap)
		return;

	ControlMap::PopInputContext(g_controlMap, RE::UserEvents::INPUT_CONTEXT_ID::kLooksMenu);
	ControlMap::PopInputContext(g_controlMap, RE::UserEvents::INPUT_CONTEXT_ID::kVirtualController);
	ControlMap::PopInputContext(g_controlMap, RE::UserEvents::INPUT_CONTEXT_ID::kThumbNav);
	ControlMap::PopInputContext(g_controlMap, RE::UserEvents::INPUT_CONTEXT_ID::kBasicMenuNav);
}

void Release(std::monostate) {
	ReleaseInputEnableLayer();
	ReleaseControlMap();
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm) {
	a_vm->BindNativeMethod("ReleaseLooksMenuLock"sv, "Release"sv, Release);
	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface * a_f4se, F4SE::PluginInfo * a_info) {
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface * a_f4se) {
	F4SE::Init(a_f4se);

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	if (papyrus)
		papyrus->Register(RegisterPapyrusFunctions);

	return true;
}
