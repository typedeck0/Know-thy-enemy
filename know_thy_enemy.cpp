/*
* arcdps combat api example
*/

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
#include <deque>
#include <cstdio>

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

/* proto/globals */
uint32_t cbtcount = 0;
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

/* arcdps exports */
void* filelog;
void* arclog;
void* arccolors;

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
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	arccolors = (void*)GetProcAddress((HMODULE)arcdll, "e5");
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
	return 0;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game) */
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	return uMsg;
}

std::unordered_map<uint32_t, uint16_t>* combatants = new std::unordered_map<uint32_t, uint16_t>();
std::unordered_map<uintptr_t, bool> ids = std::unordered_map<uintptr_t, bool>();

std::deque<std::unordered_map<uint32_t, uint16_t>*> history = std::deque<std::unordered_map<uint32_t, uint16_t>*>();
std::unordered_map<uint32_t, uint16_t>* combatants_to_display = combatants;

void record_agent(ag* agent, uint16_t instid)
{
	std::lock_guard<std::mutex>lock(mtx);
	if (ids.count(instid))
		return;
	ids.emplace(instid, true);

	uint32_t id = ((agent->prof & 0xFFFF) << 16) | (agent->elite & 0xFFFF);

	if (combatants->count(id))
	{
		combatants->at(id)++;
	}
	else
	{
		combatants->emplace(id, 1);
	}
	return;
}
bool enabled = true;
/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) 
{
	if(ev && enabled)
	{
		if (ev->is_statechange == CBTS_LOGSTART)
		{
			std::lock_guard<std::mutex>lock(mtx);
			if (!combatants->empty())
			{
				if (history.size() == 5)
				{
					if (combatants_to_display == history.back())
						combatants_to_display = combatants;
					history.back()->clear();
					delete history.back();
					history.pop_back();
				}
				history.push_front(combatants);
				combatants = new std::unordered_map<uint32_t, uint16_t>();
				if(combatants_to_display == history.front())
					combatants_to_display = combatants;
				ids.clear();
			}
			return 0;
		}
		if (ev->is_activation || ev->is_buffremove || ev->is_statechange || ev->buff || src->elite == 0xFFFFFFFF || dst->elite == 0xFFFFFFFF || src->prof == 0 || dst->prof == 0)
			return 0;
		if (src && dst)
		{
			if (src->name == nullptr)
				record_agent(src, ev->src_instid);
			else if (dst->name == nullptr)
				record_agent(dst, ev->dst_instid);
		}
	}

	return 0;
}

uintptr_t options_windows_proc(const char* windowname)
{
	if (windowname == nullptr)
		ImGui::Checkbox("Know thy enemy", &enabled);
	return 0;
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

uintptr_t imgui_proc(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc)
{
	// for (std::string* sptr : to_delete)
	// 	delete sptr;
	if (not_charsel_or_loading && enabled)
	{
		uint32_t sum = 0;
		std::vector<std::pair<uint32_t, uint16_t>> pairs = std::vector<std::pair<uint32_t, uint16_t>>();
		{
			std::lock_guard<std::mutex>lock(mtx);
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

		ImGui::Begin("Know thy enemy");
		ImGui::PushStyleColor(ImGuiCol_Text, color_array[0][4]);
		char buff[32] = {};
		snprintf(buff, 32, "%04x Total: %d", (uint16_t)combatants_to_display, sum);
		ImGui::ProgressBar(1, ImVec2(-1, 0), buff);

		for (std::pair<uint32_t, uint16_t> pair : pairs)
		{
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color_array[1][pair.first >> 16]);
			std::string display = std::to_string(pair.second).append(" ").append(std::string(get_name(pair.first)));
			ImGui::ProgressBar(pair.second / (pairs[0].second + .001f), ImVec2(-1, 0), display.c_str());
			ImGui::PopStyleColor();
		}
		ImGui::PopStyleColor();

		if( ImGui::BeginPopupContextWindow(NULL, 1))
		{
			char cbuffer[16] = {};
			snprintf(cbuffer, 16, "Current %04x  ", (uint16_t)combatants);
			if (ImGui::Button(cbuffer))
			{
				combatants_to_display = combatants;
				ImGui::CloseCurrentPopup();
			}

			for(int i = 0; i < history.size(); i++)
			{
				char hbuffer[16] = {};
				snprintf(hbuffer, 16, "History %d %04x", i+1, (uint16_t)history.at(i));
				if(ImGui::Button(hbuffer))
				{
					combatants_to_display = history.at(i);
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}
	}
	return 0;
}

/* initialize mod -- return table that arcdps will use for callbacks. exports struct and strings are copied to arcdps memory only once at init */
arcdps_exports* mod_init() {
	/* for arcdps */
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xC0FFEE;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Know thy enemy";
	arc_exports.out_build = "0.0";
	arc_exports.imgui = imgui_proc;
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.options_windows = options_windows_proc;
	//arc_exports.size = (uintptr_t)"error message if you decide to not load, sig must be 0";
	init_colors();
	log_arc((char*)"know_thy_enemy mod_init"); // if using vs2015+, project properties > c++ > conformance mode > permissive to avoid const to not const conversion error
	return &arc_exports;
}