#include <stdint.h>
#include <stdio.h>
#include <unordered_map>
#include <Windows.h>
#include <mutex>
#include <vector>
#include <algorithm>
#include "imgui/imgui.h"
#include <string>
#include <sstream>
#include <cstdio>
#include <time.h>
#include <fstream>

std::mutex mtx;

/* combat state change */
enum cbtstatechange {
	CBTS_NONE, // not used - not this kind of event
	CBTS_ENTERCOMBAT, // src_agent entered combat, dst_agent is subgroup
	CBTS_EXITCOMBAT, // src_agent left combat
	CBTS_CHANGEUP, // src_agent is now alive
	CBTS_CHANGEDEAD, // src_agent is now dead
	CBTS_CHANGEDOWN, // src_agent is now downed
	CBTS_SPAWN, // src_agent is now in game tracking range (not in realtime api)
	CBTS_DESPAWN, // src_agent is no longer being tracked (not in realtime api)
	CBTS_HEALTHUPDATE, // src_agent is at health percent. dst_agent = percent * 10000 (eg. 99.5% will be 9950) (not in realtime api)
	CBTS_LOGSTART, // log start. value = server unix timestamp **uint32**. buff_dmg = local unix timestamp. src_agent = 0x637261 (arcdps id) if evtc, species id if realtime
	CBTS_LOGEND, // log end. value = server unix timestamp **uint32**. buff_dmg = local unix timestamp. src_agent = 0x637261 (arcdps id) if evtc, species id if realtime
	CBTS_WEAPSWAP, // src_agent swapped weapon set. dst_agent = current set id (0/1 water, 4/5 land)
	CBTS_MAXHEALTHUPDATE, // src_agent has had it's maximum health changed. dst_agent = new max health (not in realtime api)
	CBTS_POINTOFVIEW, // src_agent is agent of "recording" player  (not in realtime api)
	CBTS_LANGUAGE, // src_agent is text language  (not in realtime api)
	CBTS_GWBUILD, // src_agent is game build  (not in realtime api)
	CBTS_SHARDID, // src_agent is sever shard id  (not in realtime api)
	CBTS_REWARD, // src_agent is self, dst_agent is reward id, value is reward type. these are the wiggly boxes that you get
	CBTS_BUFFINITIAL, // combat event that will appear once per buff per agent on logging start (statechange==18, buff==18, normal cbtevent otherwise)
	CBTS_POSITION, // src_agent changed, cast float* p = (float*)&dst_agent, access as x/y/z (float[3]) (not in realtime api)
	CBTS_VELOCITY, // src_agent changed, cast float* v = (float*)&dst_agent, access as x/y/z (float[3]) (not in realtime api)
	CBTS_FACING, // src_agent changed, cast float* f = (float*)&dst_agent, access as x/y (float[2]) (not in realtime api)
	CBTS_TEAMCHANGE, // src_agent change, dst_agent new team id
	CBTS_ATTACKTARGET, // src_agent is an attacktarget, dst_agent is the parent agent (gadget type), value is the current targetable state (not in realtime api)
	CBTS_TARGETABLE, // dst_agent is new target-able state (0 = no, 1 = yes. default yes) (not in realtime api)
	CBTS_MAPID, // src_agent is map id  (not in realtime api)
	CBTS_REPLINFO, // internal use, won't see anywhere
	CBTS_STACKACTIVE, // src_agent is agent with buff, dst_agent is the stackid marked active
	CBTS_STACKRESET, // src_agent is agent with buff, value is the duration to reset to (also marks inactive), pad61-pad64 buff instance id
	CBTS_GUILD, // src_agent is agent, dst_agent through buff_dmg is 16 byte guid (client form, needs minor rearrange for api form)
	CBTS_BUFFINFO, // is_flanking = probably invuln, is_shields = probably invert, is_offcycle = category, pad61 = stacking type, pad62 = probably resistance, src_master_instid = max stacks, overstack_value = duration cap (not in realtime)
	CBTS_BUFFFORMULA, // (float*)&time[8]: type attr1 attr2 param1 param2 param3 trait_src trait_self, (float*)&src_instid[2] = buff_src buff_self, is_flanking = !npc, is_shields = !player, is_offcycle = break, overstack = value of type determined by pad61 (none/number/skill) (not in realtime, one per formula)
	CBTS_SKILLINFO, // (float*)&time[4]: recharge range0 range1 tooltiptime (not in realtime)
	CBTS_SKILLTIMING, // src_agent = action, dst_agent = at millisecond (not in realtime, one per timing)
	CBTS_BREAKBARSTATE, // src_agent is agent, value is u16 game enum (active, recover, immune, none) (not in realtime api)
	CBTS_BREAKBARPERCENT, // src_agent is agent, value is float with percent (not in realtime api)
	CBTS_ERROR, // (char*)&time[32]: error string (not in realtime api)
	CBTS_TAG, // src_agent is agent, value is the id (volatile, game build dependent) of the tag, buff will be non-zero if commander
	CBTS_BARRIERUPDATE,  // src_agent is at barrier percent. dst_agent = percent * 10000 (eg. 99.5% will be 9950) (not in realtime api)
	CBTS_STATRESET,  // with arc ui stats reset (not in log), src_agent = npc id of active log
	CBTS_EXTENSION, // cbtevent with statechange byte set to this
	CBTS_APIDELAYED, // cbtevent with statechange byte set to this
	CBTS_INSTANCESTART, // src_agent is ms time at which the instance likely was started
	CBTS_TICKRATE, // every 500ms, src_agent = 25 - tickrate (when tickrate < 21)
	CBTS_LAST90BEFOREDOWN, // src_agent is enemy agent that went down, dst_agent is time in ms since last 90% (for downs contribution)
	CBTS_EFFECT, // src_agent is owner. dst_agent if at agent, else &value = float[3] xyz, &iff = float[2] xy orient, &pad61 = float[1] z orient, skillid = effectid. if is_flanking: duration = trackingid. &is_shields = uint16 duration. if effectid = 0, end &is_shields = trackingid (not in realtime api)
	CBTS_IDTOGUID, // &src_agent = 16byte persistent content guid, overstack_value is of contentlocal enum, skillid is content id  (not in realtime api)
	CBTS_UNKNOWN
};

/* is friend/foe */
enum iff {
	IFF_FRIEND,
	IFF_FOE,
	IFF_UNKNOWN
};

/* arcdps export table */
typedef struct arcdps_exports {
	uintptr_t size; /* size of exports table */
	uint32_t sig; /* pick a number between 0 and uint32_t max that isn't used by other modules */
	uint32_t imguivers; /* set this to IMGUI_VERSION_NUM. if you don't use imgui, 18000 (as of 2021-02-02) */
	const char* out_name; /* name string */
	const char* out_build; /* build string */
	void* wnd_nofilter; /* wndproc callback, fn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam), return assigned to umsg */
	void* combat; /* combat event callback, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
	void* imgui; /* ::present callback, before imgui::render, fn(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc) */
	void* options_end; /* ::present callback, appending to the end of options window in arcdps, fn() */
	void* combat_local;  /* combat event callback like area but from chat log, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
	void* wnd_filter; /* wndproc callback like wnd_nofilter above, input filered using modifiers */
	void* options_windows; /* called once per 'window' option checkbox, with null at the end, non-zero return disables arcdps drawing that checkbox, fn(char* windowname) */
} arcdps_exports;

/* combat event - see evtc docs for details, revision param in combat cb is equivalent of revision byte header */
typedef struct cbtevent {
	uint64_t time;
	uint64_t src_agent;
	uint64_t dst_agent;
	int32_t value;
	int32_t buff_dmg;
	uint32_t overstack_value;
	uint32_t skillid;
	uint16_t src_instid;
	uint16_t dst_instid;
	uint16_t src_master_instid;
	uint16_t dst_master_instid;
	uint8_t iff;
	uint8_t buff;
	uint8_t result;
	uint8_t is_activation;
	uint8_t is_buffremove;
	uint8_t is_ninety;
	uint8_t is_fifty;
	uint8_t is_moving;
	uint8_t is_statechange;
	uint8_t is_flanking;
	uint8_t is_shields;
	uint8_t is_offcycle;
	uint8_t pad61;
	uint8_t pad62;
	uint8_t pad63;
	uint8_t pad64;
} cbtevent;

/* agent short */
typedef struct ag {
	char* name; /* agent name. may be null. valid only at time of event. utf8 */
	uintptr_t id; /* agent unique identifier */
	uint32_t prof; /* profession at time of event. refer to evtc notes for identification */
	uint32_t elite; /* elite spec at time of event. refer to evtc notes for identification */
	uint32_t self; /* 1 if self, 0 if not */
	uint16_t team; /* sep21+ */
} ag;

typedef struct LinkedMem {
    uint32_t uiVersion;
    uint32_t uiTick;
    float fAvatarPosition[3];
    float fAvatarFront[3];
    float fAvatarTop[3];
    wchar_t name[256];
    float fCameraPosition[3];
    float fCameraFront[3];
    float fCameraTop[3];
    wchar_t identity[256];
    uint32_t context_len; // Despite the actual context containing more data, this value is currently 48. See "context" section below.
    unsigned char context[256];
    wchar_t description[2048];
} LinkedMem;

typedef struct MumbleContext {
    unsigned char serverAddress[28]; // contains sockaddr_in or sockaddr_in6
    uint32_t mapId;
    uint32_t mapType;
    uint32_t shardId;
    uint32_t instance;
    uint32_t buildId;
    // Additional data beyond the <context_len> bytes the game instructs Mumble to use to distinguish between instances.
    uint32_t uiState; // Bitmask: Bit 1 = IsMapOpen, Bit 2 = IsCompassTopRight, Bit 3 = DoesCompassHaveRotationEnabled, Bit 4 = Game has focus, Bit 5 = Is in Competitive game mode, Bit 6 = Textbox has focus, Bit 7 = Is in Combat
    uint16_t compassWidth; // pixels
    uint16_t compassHeight; // pixels
    float compassRotation; // radians
    float playerX; // continentCoords
    float playerY; // continentCoords
    float mapCenterX; // continentCoords
    float mapCenterY; // continentCoords
    float mapScale;
    uint32_t processId;
    uint8_t mountIndex;
} MumbleContext;

enum EMapType
{
	AutoRedirect = 0,
	CharacterCreation = 1,
	PvP = 2,
	GvG = 3,
	Instance = 4,
	PvE = 5,
	Tournament = 6,
	Tutorial = 7,
	UserTournament = 8,
	WvW_EBG = 9,
	WvW_BBL = 10,
	WvW_GBL = 11,
	WvW_RBL = 12,
	WVW_REWARD = 13,            // SCRAPPED
	WvW_ObsidianSanctum = 14,
	WvW_EdgeOfTheMists = 15,
	PvE_Mini = 16,              // Mini maps like Mistlock Sanctuary, Aerodrome, etc.
	BIG_BATTLE = 17,            // SCRAPPED
	WvW_Lounge = 18,
	//WvW = 19                  // ended up getting removed, hinting at new wvw map?
};

/* proto/globals */
uint32_t cbtcount = 0;
bool enabled = true;
bool bTitleBg = true;

// int is_wvw_state = -1;
bool mod_key1 = false;
bool mod_key2 = false;
ImGuiWindowFlags wFlags = 0;
HANDLE mumbleLinkFile = NULL;
LinkedMem* mumbleLinkedMem = nullptr;


arcdps_exports arc_exports;
char* arcvers;
void dll_init(HANDLE hModule);
void dll_exit();
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();
arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
void log_file(char* str);
void log_arc(char* str);
void save_kte_settings();

/* arcdps exports */
void* filelog;
void* arclog;
void* arccolors;
void* arccontext_0x20;
wchar_t*(*get_settings_path)();
uint64_t(*get_ui_settings)();
uint64_t(*get_key_settings)();

/* dll main -- winapi */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ulReasonForCall, LPVOID lpReserved) {
	switch (ulReasonForCall) {
	case DLL_PROCESS_ATTACH: dll_init(hModule); break;
	case DLL_PROCESS_DETACH: dll_exit(); break;

	case DLL_THREAD_ATTACH:  break;
	case DLL_THREAD_DETACH:  break;
	}
	return 1;
}

/* log to extensions tab in arcdps log window, thread/async safe */
void log_arc(char* str) {
	size_t(*log)(char*) = (size_t(*)(char*))arclog;
	if (log) (*log)(str);
	return;
}

ImVec4* color_array[5];
void init_colors()
{
	void(*colors)(ImVec4**) = (void(*)(ImVec4**))arccolors;
	if (colors) 
		(*colors)(color_array);
	return;
}

/* dll attach -- from winapi */
void dll_init(HANDLE hModule) {
	return;
}

/* dll detach -- from winapi */
void dll_exit() {
	return;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion) {
	// id3dptr is IDirect3D9* if d3dversion==9, or IDXGISwapChain* if d3dversion==11
	arcvers = arcversion;
	get_settings_path = (wchar_t*(*)())GetProcAddress((HMODULE)arcdll, "e0");
	arccontext_0x20 = (void*)GetProcAddress((HMODULE)arcdll, "e2");
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	arccolors = (void*)GetProcAddress((HMODULE)arcdll, "e5");
	get_ui_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e6");
	get_key_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e7");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void *(*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn); // on imgui 1.80+
	return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr() {
	arcvers = 0;
	return mod_release;
}

/* release mod -- return ignored */
uintptr_t mod_release() {
	FreeConsole();
	save_kte_settings();
	if (mumbleLinkedMem != nullptr)
		UnmapViewOfFile(mumbleLinkedMem);
	if (mumbleLinkFile != NULL)
		CloseHandle(mumbleLinkFile);
	return 0;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game) */
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	if ((get_ui_settings() >> 2) & 1)
	{
		wFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) 
		{
			uint64_t keys = get_key_settings();
			uint16_t* mod_key = (uint16_t*)&keys;
			if (wParam == *mod_key)
			{
				mod_key1 = true;
			}
			if (wParam == *(mod_key+1))
			{
				mod_key2 = true;
			}
		}

		if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) 
		{
			uint64_t keys = get_key_settings();
			uint16_t* mod_key = (uint16_t*)&keys;
			if (wParam == *mod_key)
			{
				mod_key1 = false;
			}
			if (wParam == *(mod_key+1))
			{
				mod_key2 = false;
			}
		}
		if (mod_key1 && mod_key2)
			wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	}
	else
		wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	return uMsg;
}


typedef std::unordered_map<uint32_t, uint16_t> id_umap;
typedef std::unordered_map<uint16_t, std::vector<id_umap>> team_history;

std::unordered_map<uint16_t, bool> ids = std::unordered_map<uint16_t, bool>();

team_history history = team_history();
int history_radio_state = 0;
int combatants_idx = 0;
int combatants_disp_idx = 0;
uint16_t selected_team = 0;


void record_agent(ag* agent, uint16_t instid)
{
	if (agent->team == 0)
		return;
	std::lock_guard<std::mutex>lock(mtx);
	if (history.find(agent->team) == history.end())
	{
		history.emplace(agent->team, std::vector<id_umap>());
		for(int i = 0; i < 6; i++)
		{
			history.at(agent->team).push_back(id_umap());
		}
	}

	if (ids.find(instid) != ids.end())
		return;
	ids.emplace(instid, true);

	uint32_t id = ((agent->prof & 0xFFFF) << 16) | (agent->elite & 0xFFFF);

	if (history.at(agent->team)[combatants_idx].find(id) != history.at(agent->team)[combatants_idx].end())
	{
		history.at(agent->team)[combatants_idx].at(id)++;
	}
	else
	{
		history.at(agent->team)[combatants_idx].emplace(id, 1);
	}

	return;
}

// int check_wvw()
// {
// 	int result = 0;

// 	if (mumbleLinkFile == NULL)
// 	{
// 		mumbleLinkFile = CreateFileMapping(
// 					INVALID_HANDLE_VALUE,    // use paging file
// 					NULL,                    // default security
// 					PAGE_READWRITE,          // read/write access
// 					0,                       // maximum object size (high-order DWORD)
// 					sizeof(LinkedMem) + 256 + 4096,                // maximum object size (low-order DWORD)
// 					TEXT("MumbleLink"));                 // name of mapping object
// 	}
// 	if (mumbleLinkFile == NULL)
// 	{
// 		return -1;
// 	}

// 	if (mumbleLinkedMem == nullptr)
// 		mumbleLinkedMem = (LinkedMem*) MapViewOfFile(mumbleLinkFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem)+256+4096);

// 	if (mumbleLinkedMem == NULL)
// 	{
// 		CloseHandle(mumbleLinkFile);
// 		return -1;
// 	}

// 	while (!mumbleLinkedMem->uiTick)
// 		Sleep(100);

// 	MumbleContext* context = (MumbleContext*)(&mumbleLinkedMem->context[0]);

// 	switch (context->mapType)
// 	{
// 	case EMapType::WvW_BBL:
// 	case EMapType::WvW_EBG:
// 	case EMapType::WvW_EdgeOfTheMists:
// 	case EMapType::WvW_GBL:
// 	case EMapType::WvW_Lounge:
// 	case EMapType::WvW_ObsidianSanctum:
// 	case EMapType::WvW_RBL:
// 		result = 1;
// 		break;
// 	default:
// 		result = 0;
// 		break;
// 	}

// 	return result;
// }

bool log_ended = false;

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) 
{
	if(enabled)
	{
		if (ev)
		{
			if (ev->is_statechange == CBTS_LOGEND)
				log_ended = true;
			if (ev->is_activation || ev->is_buffremove || ev->is_statechange || ev->buff || src->elite == 0xFFFFFFFF || dst->elite == 0xFFFFFFFF || src->prof == 0 || dst->prof == 0)
				return 0;
			if (src && dst)
			{
				if (log_ended && (src->name == nullptr || dst->name == nullptr))
				{
					std::lock_guard<std::mutex>lock(mtx);
					combatants_idx = (combatants_idx + 1) % 6;
					for (auto team : history)
					{
						if (!team.second[combatants_idx].empty())
						{
							history.at(team.first)[combatants_idx].clear();
						}
						ids.clear();
						combatants_disp_idx = combatants_idx;
						history_radio_state = 0;
					}
					log_ended = false;
				}

				if (src->name == nullptr)
				{
					record_agent(src, ev->src_instid);
				}
				else if (dst->name == nullptr)
				{
					record_agent(dst, ev->dst_instid);
				}
			}
		}
	}
	return 0;
}

void options_end_proc(const char* windowname)
{
	ImGui::Checkbox("Know thy enemy##1cb", &enabled);
	ImGui::Separator();
	if (ImGui::Button("Reset settings"))
	{
		enabled = true;
		wFlags = 0;
		bTitleBg = true;
		std::wstring path = std::wstring(get_settings_path());
		std::string cpath(path.begin(), path.end());
		cpath = cpath.substr(0, cpath.find_last_of("\\")+1);
		cpath.append("know_thy_enemy_settings.txt");
		std::fstream file(cpath.c_str(), std::fstream::out | std::fstream::trunc);
		if (file.good())
		{
			file << "enabled=" << (enabled ? '1' : '0') << "\n";
			file << "wFlags=" << std::to_string(wFlags) << "\n";
			file << "titleTrans=" << (bTitleBg ? '1' : '0') << "\n";
		}
		file.close();
	}
}

void options_windows_proc(const char* windowname)
{
	// log_arc((char*)windowname);
}


char* get_name(uint32_t id)
{
	switch (id & 0xFFFF)
	{
	case 0:
		switch (id >> 16)
		{
		case 1: return "Guardian";
		case 2: return "Warrior";
		case 3: return "Engineer";
		case 4: return "Ranger";
		case 5: return "Thief";
		case 6: return "Elementalist";
		case 7: return "Mesmer";
		case 8: return "Necromancer";
		case 9: return "Revenant";
		default: return "Unknown";
		}
	case 5:	return "Druid";
	case 7:	return "Daredevil";
	case 18: return "Berserker";
	case 27: return "Dragonhunter";
	case 34: return "Reaper";
	case 40: return "Chronomancer";
	case 43: return "Scrapper";
	case 48: return "Tempest";
	case 52: return "Herald";
	case 55: return "Soulbeast";
	case 56: return "Weaver";
	case 57: return "Holosmith";
	case 58: return "Deadeye";
	case 59: return "Mirage";
	case 60: return "Scourge";
	case 61: return "Spellbreaker";
	case 62: return "Firebrand";
	case 63: return "Renegade";
	case 64: return "Harbinger";
	case 65: return "Willbender";
	case 66: return "Virtuoso";
	case 67: return "Catalyst";
	case 68: return "Bladesworn";
	case 69: return "Vindicator";
	case 70: return "Mechanist";
	case 71: return "Specter";
	case 72: return "Untamed";
	default: return "Unknown";
	}
}

std::vector<std::string> strings = std::vector<std::string>();

void new_string_int(char * format, int val)
{
	strings.push_back(std::string(32, 0));
	snprintf(&strings.back()[0], 32, format, val);
}

void imgui_team_combatants(std::pair<uint16_t, std::vector<id_umap>> team)
{
	selected_team = team.first;
	id_umap* combatants_to_display = &(history.at(selected_team)[combatants_disp_idx]);
	uint32_t sum = 0;
	std::vector<std::pair<uint32_t, uint16_t>> pairs = std::vector<std::pair<uint32_t, uint16_t>>();
	{
		for (auto itr = combatants_to_display->begin(); itr != combatants_to_display->end(); ++itr)
		{
			sum += (*itr).second;
			pairs.push_back(*itr);
		}
	}

	std::sort(pairs.begin(), pairs.end(), [=](std::pair<uint32_t, uint16_t>& a, std::pair<uint32_t, uint16_t>& b)
	{
		return a.second > b.second;
	}
	);

	ImGui::PushStyleColor(ImGuiCol_Text, color_array[0][4]);
	new_string_int("Total: %d", sum);
	ImGui::ProgressBar(1, ImVec2(-1, 0), strings.back().c_str());

	for (std::pair<uint32_t, uint16_t> pair : pairs)
	{
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color_array[1][pair.first >> 16]);
		strings.push_back(std::to_string(pair.second).append(" ").append(std::string(get_name(pair.first))));
		ImGui::ProgressBar(pair.second / (pairs[0].second + .001f), ImVec2(-1, 0), strings.back().c_str());
		ImGui::PopStyleColor();
	}
	ImGui::PopStyleColor();
}

void add_new_team_name(uint16_t team_id)
{
	switch (team_id)
	{
	case 432:
		strings.push_back("Blue");
		break;
	case 2739:
		strings.push_back("Green");
		break;
	case 705:
		strings.push_back("Red");
		break;
	default:
		new_string_int("Team %d", team_id);
		break;
	}
}

uintptr_t imgui_proc(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc)
{
	if (not_charsel_or_loading && enabled)
	{
		strings.clear();
		bool pushed = false;
		if (!bTitleBg)
		{
			ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg); //xyzw == RGBA
			ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(color.x, color.y, color.z, 0.0)); 
			ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(color.x, color.y, color.z, 0.0)); 
			pushed = true;
		}
		if (ImGui::Begin("Know thy enemy", &enabled, wFlags))
		{
			if (ImGui::BeginTabBar("MyTabBar", 0))
			{
				std::lock_guard<std::mutex>lock(mtx);
				for (auto team : history)
				{
					add_new_team_name(team.first);
					if (ImGui::BeginTabItem(strings.back().c_str()))
					{
						imgui_team_combatants(team);
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}

			if( ImGui::BeginPopupContextWindow(NULL, 1))
			{
				if (ImGui::BeginMenu("History"))
				{
					if(selected_team == 0)
					{
						ImGui::Text("No data...");
					}
					else
					{
						ImGuiStyle style = ImGui::GetStyle();
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
						strings.push_back(std::string(32, 0));
						snprintf(&strings.back()[0], 32, " Current ");
						if (ImGui::RadioButton(strings.back().c_str(), &history_radio_state, 0))
						{
							history_radio_state = 0;
							combatants_disp_idx = combatants_idx;
							ImGui::CloseCurrentPopup();
						}
						int order_idx = combatants_idx - 1;
						if(order_idx < 0)
							order_idx = 5;
						for(int i = 0; i < 5; i++)
						{
							new_string_int("History %d", i+1);
							if(ImGui::RadioButton(strings.back().c_str(), &history_radio_state, i+1))
							{
								history_radio_state = i+1;
								combatants_disp_idx = order_idx;
								ImGui::CloseCurrentPopup();
							}
							order_idx--;
							if(order_idx < 0)
								order_idx = 5;
						}
						ImGui::PopStyleVar();
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Style"))
				{
					ImGuiStyle style = ImGui::GetStyle();
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
					bool bTitlebar = (wFlags & ImGuiWindowFlags_NoTitleBar) == 0;
					if (ImGui::Checkbox("title bar", &bTitlebar))
					{
						wFlags ^= ImGuiWindowFlags_NoTitleBar;
					}
					bool bScrollbar = (wFlags & ImGuiWindowFlags_NoScrollbar) == 0;
					if (ImGui::Checkbox("scroll bar", &bScrollbar))
					{
						wFlags ^= ImGuiWindowFlags_NoScrollbar;
					}
					bool bBackground = (wFlags & ImGuiWindowFlags_NoBackground) == 0;
					if (ImGui::Checkbox("background", &bBackground))
					{
						wFlags ^= ImGuiWindowFlags_NoBackground;
					}
					if (ImGui::Checkbox("title bar background", &bTitleBg))
					{
					}
					ImGui::PopStyleVar();
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();
		if (pushed)
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
		}
	}
	return 0;
}

void save_kte_settings()
{
	std::wstring path = std::wstring(get_settings_path());
	std::string cpath(path.begin(), path.end());
	cpath = cpath.substr(0, cpath.find_last_of("\\")+1);
	cpath.append("know_thy_enemy_settings.txt");
	std::fstream file(cpath.c_str(), std::fstream::out | std::fstream::trunc);
	if (file.good())
	{
		file << "enabled=" << (enabled ? '1' : '0') << "\n";
		file << "wFlags=" << std::to_string(wFlags) << "\n";
		file << "titleTrans=" << (bTitleBg ? '1' : '0') << "\n";
	}
	file.close();
}


void init_kte_settings()
{
	std::wstring path = std::wstring(get_settings_path());
	std::string cpath(path.begin(), path.end());
	cpath = cpath.substr(0, cpath.find_last_of("\\")+1);
	cpath.append("know_thy_enemy_settings.txt");
	std::fstream file(cpath.c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
	std::string line;
	bool success = false;
	if (file.good())
	{
		while (std::getline(file, line))
		{
			auto sep = line.find_first_of('=');
			std::string key = line.substr(0, sep);
			if (key.compare("enabled") == 0)
			{
				char val = line.substr(sep+1, line.size() - key.size() - 1)[0];
				enabled = (val == '1');
			}
			else if(key.compare("wFlags") == 0)
			{
				wFlags = std::stoi(line.substr(sep+1, line.size() - key.size() - 1));
			}
			else if (key.compare("titleTrans") == 0)
			{
				char val = line.substr(sep+1, line.size() - key.size() - 1)[0];
				bTitleBg = (val == '1');
			}

		}
	}
	if (!success)
	{
		file.open(cpath.c_str(), std::fstream::in | std::fstream::out | std::fstream::trunc);
	}
	file.close();
}

/* initialize mod -- return table that arcdps will use for callbacks. exports struct and strings are copied to arcdps memory only once at init */
arcdps_exports* mod_init() {
	/* for arcdps */
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xC0FFEE;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Know thy enemy";
	arc_exports.out_build = "2.5.1";
	arc_exports.imgui = imgui_proc;
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.options_end = options_end_proc;
	arc_exports.options_windows = options_windows_proc;
	//arc_exports.size = (uintptr_t)"error message if you decide to not load, sig must be 0";
	init_colors();
	init_kte_settings();
	combatants_idx = 0;
	combatants_disp_idx = 0;
	log_arc((char*)"know_thy_enemy mod_init"); // if using vs2015+, project properties > c++ > conformance mode > permissive to avoid const to not const conversion error
	return &arc_exports;
}