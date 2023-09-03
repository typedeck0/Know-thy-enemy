#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mutex>
#include <algorithm>
#include "imgui/imgui.h"
#include <string>
#include <sstream>
#include <fstream>
#include <array>
#include <unordered_map>
#include <atomic>
#include <time.h>

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
	CBTS_LOGSTART, // log start. value = server unix timestamp **uint32**. buff_dmg = local unix timestamp
	CBTS_LOGEND, // log end. value = server unix timestamp **uint32**. buff_dmg = local unix timestamp
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
	CBTS_EFFECT, // retired, not used since 230716+
	CBTS_IDTOGUID, // &src_agent = 16byte persistent content guid, overstack_value is of contentlocal enum, skillid is content id  (not in realtime api)
	CBTS_LOGNPCUPDATE, // log npc update. value = server unix timestamp **uint32**. buff_dmg = local unix timestamp. src_agent = species id. dst_agent = agent, flanking = is gadget
	CBTS_IDLEEVENT, // internal use, won't see anywhere
	CBTS_EXTENSIONCOMBAT, // cbtevent with statechange byte set to this, treats skillid as skill for evtc skill table
	CBTS_FRACTALSCALE, // src_agent = fractal scale
	CBTS_EFFECT2, // src_agent is owner. dst_agent if at agent, else &value = float[3] xyz. &iff = uint32 duraation. &buffremove = uint32 trackable id. &is_shields = int16[3] orientation, values are original*1000 clamped to int16 (not in realtime api)
	CBTS_UNKNOWN
};

/* is friend/foe */
enum iff {
	IFF_FRIEND,
	IFF_FOE,
	IFF_UNKNOWN
};

/* arcdps export table */
struct arcdps_exports {
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
};

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

enum class HackedMapIds
{
	WVW_LOUNGE = 0x0523,
	WVW_EBG    = 0x0026,
	WVW_GBL    = 0x005F,
	WVW_BBL    = 0x0060,
	WVW_RBL    = 0x044B
};

namespace DATA_ARRAY
{
enum LAYOUT : uint8_t
{
	Guardian,
	Dragonhunter,
	Firebrand,
	Willbender,
	Warrior,
	Berserker,
	Spellbreaker,
	Bladesworn,
	Engineer,
	Scrapper,
	Holosmith,
	Mechanist,
	Ranger,
	Druid,
	Soulbeast,
	Untamed,
	Thief,
	Daredevil,
	Deadeye,
	Specter,
	Elementalist,
	Tempest,
	Weaver,
	Catalyst,
	Mesmer,
	Chronomancer,
	Mirage,
	Virtuoso,
	Necromancer,
	Reaper,
	Scourge,
	Harbinger,
	Revenant,
	Herald,
	Renegade,
	Vindicator,
	Unknown,
	LENGTH
};
}

const std::array<const char[16], DATA_ARRAY::LENGTH> pe_name_lut = {
	"Guardian",
	"Dragonhunter",
	"Firebrand",
	"Willbender",
	"Warrior",
	"Berserker",
	"Spellbreaker",
	"Bladesworn",
	"Engineer",
	"Scrapper",
	"Holosmith",
	"Mechanist",
	"Ranger",
	"Druid",
	"Soulbeast",
	"Untamed",
	"Thief",
	"Daredevil",
	"Deadeye",
	"Specter",
	"Elementalist",
	"Tempest",
	"Weaver",
	"Catalyst",
	"Mesmer",
	"Chronomancer",
	"Mirage",
	"Virtuoso",
	"Necromancer",
	"Reaper",
	"Scourge",
	"Harbinger",
	"Revenant",
	"Herald",
	"Renegade",
	"Vindicator",
	"Unknown",
};

const std::array<const char[4], DATA_ARRAY::LENGTH> pe_short_name_lut = {
	"Gdn",
	"Dgh",
	"Fbd",
	"Wbd",
	"War",
	"Brs",
	"Spb",
	"Bds",
	"Eng",
	"Scr",
	"Hls",
	"Mec",
	"Rgr",
	"Dru",
	"Slb",
	"Unt",
	"Thf",
	"Dar",
	"Ded",
	"Spe",
	"Ele",
	"Tmp",
	"Wea",
	"Cat",
	"Mes",
	"Chr",
	"Mir",
	"Vir",
	"Nec",
	"Rea",
	"Scg",
	"Har",
	"Rev",
	"Her",
	"Ren",
	"Vin",
	"Ukn"
};

struct s_profelite {
	uint8_t prof = 0;
	uint8_t elite = 0;
	uint8_t idx = 0;
	uint8_t count = 0;
	s_profelite() {}
	s_profelite(uint8_t _prof, uint8_t _elite)
	{
		prof = _prof;
		elite = _elite;
		switch (_elite)
		{
		case 0:
			switch (_prof)
			{
			case 1: 
				idx = DATA_ARRAY::Guardian;
				return;
			case 2: 
				idx = DATA_ARRAY::Warrior;
				return;
			case 3: 
				idx = DATA_ARRAY::Engineer;
				return;
			case 4: 
				idx = DATA_ARRAY::Ranger;
				return;
			case 5:  
				idx = DATA_ARRAY::Thief;
				return;
			case 6:  
				idx = DATA_ARRAY::Elementalist;
				return;
			case 7:  
				idx = DATA_ARRAY::Mesmer;
				return;
			case 8:  
				idx = DATA_ARRAY::Necromancer;
				return;
			case 9:  
				idx = DATA_ARRAY::Revenant;
				return;
			default: 
				idx = DATA_ARRAY::Unknown;
				return;
			}
		case 5:	
			idx = DATA_ARRAY::Druid;
			return;
		case 7:	
			idx = DATA_ARRAY::Daredevil;
			return;
		case 18:
			idx = DATA_ARRAY::Berserker;
			return;
		case 27:
			idx = DATA_ARRAY::Dragonhunter;
			return;
		case 34:
			idx = DATA_ARRAY::Reaper;
			return;
		case 40:
			idx = DATA_ARRAY::Chronomancer;
			return;
		case 43:
			idx = DATA_ARRAY::Scrapper;
			return;
		case 48:
			idx = DATA_ARRAY::Tempest;
			return;
		case 52:
			idx = DATA_ARRAY::Herald;
			return;
		case 55:
			idx = DATA_ARRAY::Soulbeast;
			return;
		case 56:
			idx = DATA_ARRAY::Weaver;
			return;
		case 57:
			idx = DATA_ARRAY::Holosmith;
			return;
		case 58:
			idx = DATA_ARRAY::Deadeye;
			return;
		case 59:
			idx = DATA_ARRAY::Mirage;
			return;
		case 60:
			idx = DATA_ARRAY::Scourge;
			return;
		case 61:
			idx = DATA_ARRAY::Spellbreaker;
			return;
		case 62:
			idx = DATA_ARRAY::Firebrand;
			return;
		case 63:
			idx = DATA_ARRAY::Renegade;
			return;
		case 64:
			idx = DATA_ARRAY::Harbinger;
			return;
		case 65:
			idx = DATA_ARRAY::Willbender;
			return;
		case 66:
			idx = DATA_ARRAY::Virtuoso;
			return;
		case 67:
			idx = DATA_ARRAY::Catalyst;
			return;
		case 68:
			idx = DATA_ARRAY::Bladesworn;
			return;
		case 69:
			idx = DATA_ARRAY::Vindicator;
			return;
		case 70:
			idx = DATA_ARRAY::Mechanist;
			return;
		case 71:
			idx = DATA_ARRAY::Specter;
			return;
		case 72:
			idx = DATA_ARRAY::Untamed;
			return;
		default:
			idx = DATA_ARRAY::Unknown;
			return;
		}
	}
};

enum combat_state {
	IN_BATTLE,
	OUT_BATTLE,
};

enum display_state {
	BATTLE,
	HISTORY_TAB,
	HISTORY_COL,
	NONE,
};

struct s_team_battle {
	uint8_t total = 0;
	uint8_t total_hit = 0;
	std::array<s_profelite, DATA_ARRAY::LENGTH> profelites = std::array<s_profelite, DATA_ARRAY::LENGTH>();
};

struct custom_team {
	unsigned int id = 0;
	std::array<char, 16> name = {0};
};

std::mutex mtx;

/* proto/globals */
struct settings {
	uint32_t magic = 0xC0FFEE;
	bool bEnabled = true;
	ImGuiWindowFlags wFlags = 0;
	bool bToShow = false;
	bool bTitleBg = true;
	bool bShowColumns = false;
	bool bShortNames = false;
	unsigned int red_team = 705;
	unsigned int green_team = 2739;
	unsigned int blue_team = 432;
	bool bHideInCombat = false;
	custom_team cteam1;
	custom_team cteam2;
	custom_team cteam3;
};

struct ArcContext 
{
	struct AgentManager 
	{
		struct AgentDataList 
		{
			struct AgentBlock 
			{
				AgentDataList* agentDataList;
				AgentBlock* prev;
				AgentBlock* next;
				unsigned char* agent;
				void* flags;
				void* p0;
				void* p1;
				unsigned short length;
				unsigned short cur_i;
				unsigned short dataSize;
			};
			ArcContext* arcContext;
			AgentBlock* tail;
			AgentBlock* head;
		};
		ArcContext* arcContext;
		void* p0;
		void* p1;
		AgentDataList* agentDataList;
	};
	AgentManager* agentManager;
	unsigned char pad[0x628-sizeof(AgentManager*)];
	short map_id;
};

const uint8_t MAX_HISTORY_SIZE = 8; //power of 2
const uint8_t MAX_STRING_SIZE = 32;
const uint8_t MAX_TOTAL_STRINGS = 64;

// int is_wvw_state = -1;
bool mod_key1 = false;
bool mod_key2 = false;
uint16_t tab_teamid = 0;
bool override_tab_max_switch = false;

char* arcvers;
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();

/* arcdps exports */
size_t(*arclog)(char*);
void(*arccolors)(ImVec4**);
const char*(*arccontext_0x508)();
wchar_t*(*get_settings_path)();
uint64_t(*get_ui_settings)();
uint64_t(*get_key_settings)();

ArcContext* arccontext = nullptr;

std::unordered_map<uint32_t, bool> ids = std::unordered_map<uint32_t, bool>();
std::unordered_map<uint16_t, std::array<s_team_battle, MAX_HISTORY_SIZE>> team_history_map = std::unordered_map<uint16_t, std::array<s_team_battle, MAX_HISTORY_SIZE>>();
std::array<std::array<char, 32>, MAX_HISTORY_SIZE> history_labels;

settings kte_settings;

int history_radio_state = 0;
int cur_history_idx = 0;
int history_to_disp_idx = 0;

/* dll attach -- from winapi */
void dll_init(const HANDLE hModule) {
	return;
}

/* dll detach -- from winapi */
void dll_exit() {
	return;
}

/* dll main -- winapi */
BOOL APIENTRY DllMain(const HANDLE hModule, const DWORD ulReasonForCall, const LPVOID lpReserved) {
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
	if (arclog) 
		arclog(str);
	return;
}

ImVec4* color_array[5];
void init_colors()
{
	if (arccolors) 
		arccolors(color_array);
	return;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game) */
uintptr_t mod_wnd(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) 
{
	if ((get_ui_settings() >> 2) & 1)
	{
		kte_settings.wFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
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

		else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) 
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
			kte_settings.wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	}
	else
		kte_settings.wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	return uMsg;
}

bool isWvw()
{
	if (arccontext == nullptr)
		return false;
	switch ((HackedMapIds)arccontext->map_id)
	{
	case HackedMapIds::WVW_BBL:
	case HackedMapIds::WVW_EBG:
	case HackedMapIds::WVW_GBL:
	case HackedMapIds::WVW_RBL:
	case HackedMapIds::WVW_LOUNGE:
		return true;
	default:
		return false;
	}
}

unsigned int logstart_time = 0xFFFFFFFF;
unsigned int last_cbt_evt = 0;
unsigned long long log_end_id = 0;
combat_state cmb_state = combat_state::OUT_BATTLE;
bool log_start = false;

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
uintptr_t mod_combat(const cbtevent* ev, const ag* src, const ag* dst, const char* skillname, const uint64_t id, const uint64_t revision) 
{
	if (src)
	{
		arccontext = *(ArcContext**)(((char*)src) - 0x28);
	}
	if (!ev)
	{
		if (!src->elite)
		{
			if(src->prof) //add
			{
				if(dst->self)
				{
					// char buf[64] = {0};
					// snprintf(buf, 64, "%llx|%llx", src->id, dst->id);
					// log_arc(buf);
					kte_settings.bToShow = isWvw();
				}
			}
		}
	}
	else if(ev && kte_settings.bEnabled && kte_settings.bToShow)
	{
		if (ev->is_statechange == CBTS_LOGSTART)
		{
			logstart_time = GetTickCount();
			log_end_id = 1;
			log_start = true;
		}
		else if (ev->is_statechange == CBTS_LOGEND)
		{
			_strtime_s(&history_labels[cur_history_idx][0], 32);
			snprintf(&history_labels[cur_history_idx][8], 32-9, " (%ds)", (GetTickCount() - logstart_time)/1000);
			cmb_state = combat_state::OUT_BATTLE;
			log_end_id = id;
			log_start = false;
		}
		if (ev->is_activation || ev->is_buffremove || ev->is_statechange || ev->buff || src->elite == 0xFFFFFFFF || dst->elite == 0xFFFFFFFF || src->prof == 0 || dst->prof == 0)
			return 0;
		if (src && dst && ev->value != 0 && id > log_end_id)
		{
			std::lock_guard<std::mutex>lock(mtx);
			if (log_start)
				cmb_state = combat_state::IN_BATTLE;
			if (GetTickCount() - last_cbt_evt < 200)
				return 0;

			last_cbt_evt = GetTickCount();

			bool all_zero = true;
			for (auto& team : team_history_map)
				all_zero = all_zero && team.second[cur_history_idx].total == 0;
			if (log_end_id == 1 && !all_zero)
			{
				log_end_id = 0;
				cur_history_idx = (cur_history_idx + 1) & (MAX_HISTORY_SIZE - 1);
				for (auto& team : team_history_map)
				{
					if (team.second[cur_history_idx].total != 0)
					{
						team.second[cur_history_idx].total = 0;
						team.second[cur_history_idx].total_hit = 0;
						for(auto& profelite : team.second[cur_history_idx].profelites)
						{
							profelite.count = 0;
						}
					}
				}
				history_to_disp_idx = cur_history_idx;
				history_radio_state = 0;
				ids.clear();
				override_tab_max_switch = false;
			}

			if (arccontext == nullptr)
				return 0;

			if (arccontext->agentManager == 0)
				return 0;
			if (arccontext->agentManager->agentDataList == 0)
				return 0;
			if (arccontext->agentManager->agentDataList->head == 0)
				return 0;
			if (arccontext->agentManager->agentDataList->head->agent == 0)
				return 0;

			ArcContext::AgentManager::AgentDataList::AgentBlock* cur_agents = arccontext->agentManager->agentDataList->head;
			char buf[32];
			while (cur_agents != 0)
			{
				int i = cur_agents->length - 1;
				unsigned short size = cur_agents->dataSize;
				// snprintf(buf, 32, "%d %d\n", i, size);
				// log_arc(buf);
				for (; i >= 0; i--)
				{
					unsigned char* cur_agent = cur_agents->agent + (i*size);
					if (*(unsigned short*)(cur_agent + 0x85a) != 0x3000)
						continue;
					if (*(unsigned short*)(cur_agent + 0x590) == 0 && *(unsigned short*)(cur_agent + 0x570) == 0)//i_hit == null && they_hit == null
						continue;
					char * name = *(char **)(cur_agent + 0x10);
					bool isFoe = name != 0 && name[2] > 47 && name[2] < 58;
					if (!isFoe)
						continue;
					unsigned short instid = *(unsigned short*)(cur_agent + 0x85c);
					unsigned short team_id = *(unsigned short*)(cur_agent + 0x878); //-4
					unsigned short prof = *(unsigned short*)(cur_agent + 0x87c); //0
					unsigned short elite = *(unsigned short*)(cur_agent + 0x87e); //2
					if (team_id == 0 || team_id == 0xFFFF)
						continue;

					auto& team = team_history_map[team_id];
					bool iHit = false;//(src->self & 1)*is_dst;

					if (ids.find(instid) != ids.end()) //if found
					{
						team[cur_history_idx].total_hit += (iHit && !(ids[instid]));
						ids[instid] = (iHit && !(ids[instid])) || ids[instid];
						continue; //dont process found
					}
					ids[instid] = iHit;
					team[cur_history_idx].total_hit += ids[instid];

					s_profelite pe(prof & 0xFF, elite & 0xFF);

					if (team[cur_history_idx].profelites[pe.idx].prof == 0)
					{
						team[cur_history_idx].profelites[pe.idx] = pe;
					}
						
					team[cur_history_idx].profelites[pe.idx].count++;
					team[cur_history_idx].total++;
				}
				cur_agents = cur_agents->next;
			}
		}
	}
	return 0;
}

void options_end_proc(const char* windowname)
{
	ImGui::Checkbox("Know thy enemy##1cb", &kte_settings.bEnabled);

	ImGui::Text("Red team id:  ");
	ImGui::SameLine();
	ImGui::InputInt("##redteamid", (int *)&kte_settings.red_team, 0, 0);
	ImGui::Text("Green team id:");
	ImGui::SameLine();
	ImGui::InputInt("##greenteamid", (int *)&kte_settings.green_team, 0, 0);
	ImGui::Text("Blue team id: ");
	ImGui::SameLine();
	ImGui::InputInt("##blueteamid", (int *)&kte_settings.blue_team, 0, 0);

	ImGui::Text("Custom 1 name:");
	ImGui::SameLine();
	ImGui::InputText("##cteam1name", kte_settings.cteam1.name.data(), 16);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam1id", (int *)&kte_settings.cteam1.id, 0, 0);

	ImGui::Text("Custom 2 name:");
	ImGui::SameLine();
	ImGui::InputText("##cteam2name", kte_settings.cteam2.name.data(), 16);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam2id", (int *)&kte_settings.cteam2.id, 0, 0);

	ImGui::Text("Custom 3 name:");
	ImGui::SameLine();
	ImGui::InputText("##cteam3name", kte_settings.cteam3.name.data(), 16);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam3id", (int *)&kte_settings.cteam3.id, 0, 0);

	ImGui::NewLine();
	ImGui::Separator();
	if (ImGui::Button("Reset settings"))
	{
		kte_settings = settings();
		std::wstring path = std::wstring(get_settings_path());
		path = path.substr(0, path.find_last_of(L"\\")+1);
		path.append(L"know_thy_enemy_settings.txt");
		std::fstream file(path.c_str(), std::fstream::out | std::fstream::trunc | std::ios::binary);
		if (file.good())
		{
			file.write((char*)&kte_settings, sizeof(settings));
		}
		file.close();
	}
}

void options_windows_proc(const char* windowname)
{
	// log_arc((char*)windowname);
}

void draw_bar(const float frac, const char* text, const ImVec4& color)
{
	ImVec2 upper_left = ImGui::GetCursorScreenPos();
	ImVec2 lower_right = ImVec2(upper_left.x + (ImGui::GetContentRegionAvail().x*frac), upper_left.y + ImGui::GetTextLineHeight() + 2);
	//ImGui::ProgressBar(frac, ImVec2(-1, 0), "");
	ImGui::GetWindowDrawList()->AddRectFilled(upper_left, lower_right, ImGui::ColorConvertFloat4ToU32(color));

	ImVec2 text_start = ImGui::GetCursorPos();
	text_start.y += 1; //middle of bar
	float inner_pad = 5; // (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x) * 0.5;
	text_start.x += inner_pad;
	ImVec2 text_shadow = text_start;
	text_shadow.x += 1;
	text_shadow.y += 1;
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,.6));
	ImGui::SetCursorPos(text_shadow);
	ImGui::TextUnformatted(text, text + strlen(text));
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(text_start);
	ImGui::TextUnformatted(text, text + strlen(text));
	text_start.x -= inner_pad;

	text_start.y += ImGui::GetTextLineHeight() + 3; //end of bar + 2 pad
	ImGui::SetCursorPos(text_start);
}

void draw_history_menu()
{
	if(team_history_map.size() == 0)
	{
		ImGui::Text("No data...");
	}
	else
	{
		if (ImGui::RadioButton("Current", history_radio_state == 0))
		{
			history_radio_state = 0;
			history_to_disp_idx = cur_history_idx;
			ImGui::CloseCurrentPopup();
		}
		int past_history_idx = (cur_history_idx - 1 + MAX_HISTORY_SIZE) & (MAX_HISTORY_SIZE - 1);
		for(int i = 0; i < MAX_HISTORY_SIZE-1; i++)
		{
			if(ImGui::RadioButton(&history_labels[past_history_idx][0], history_radio_state == (i+1)))
			{
				history_radio_state = i+1;
				history_to_disp_idx = past_history_idx;
				ImGui::CloseCurrentPopup();
			}
			past_history_idx = (past_history_idx - 1 + MAX_HISTORY_SIZE) & (MAX_HISTORY_SIZE - 1);
		}
	}
}

void draw_style_menu()
{
	bool bTitlebar = (kte_settings.wFlags & ImGuiWindowFlags_NoTitleBar) == 0;
	if (ImGui::Checkbox("title bar", &bTitlebar))
	{
		kte_settings.wFlags ^= ImGuiWindowFlags_NoTitleBar;
	}
	bool bScrollbar = (kte_settings.wFlags & ImGuiWindowFlags_NoScrollbar) == 0;
	if (ImGui::Checkbox("scroll bar", &bScrollbar))
	{
		kte_settings.wFlags ^= ImGuiWindowFlags_NoScrollbar;
	}
	bool bBackground = (kte_settings.wFlags & ImGuiWindowFlags_NoBackground) == 0;
	if (ImGui::Checkbox("background", &bBackground))
	{
		kte_settings.wFlags ^= ImGuiWindowFlags_NoBackground;
	}
	ImGui::Checkbox("title bar background", &kte_settings.bTitleBg);
	ImGui::Checkbox("use columns", &kte_settings.bShowColumns);
	ImGui::Checkbox("use short names", &kte_settings.bShortNames);
	ImGui::Checkbox("hide while in combat", &kte_settings.bHideInCombat);
}

void imgui_team_class_bars(const s_team_battle& team_combatants_to_disp)
{
	if (team_combatants_to_disp.total == 0)
	{
		if (kte_settings.bShortNames)
			draw_bar(1.f, "00 Tot", ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));
		else
			draw_bar(1.f, "00 Total", ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));
		return;
	}
	std::array<s_profelite, DATA_ARRAY::LENGTH> combatants_to_disp = team_combatants_to_disp.profelites;
	uint8_t total = team_combatants_to_disp.total;
	uint8_t total_hit = team_combatants_to_disp.total_hit;
	std::sort(combatants_to_disp.begin(), combatants_to_disp.end(), [=](s_profelite& a, s_profelite& b)
	{
		return a.count > b.count;
	});

	ImGui::PushStyleColor(ImGuiCol_Text, color_array[0][4]);

	char temp[32] = {0};
	if (kte_settings.bShortNames)
		snprintf(temp, 32, "%02d Tot", total);
	else
		snprintf(temp, 32, "%02d Total", total);
	draw_bar(1.f, temp, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));

	uint16_t cur_max = combatants_to_disp[0].count;
	for (auto& profelite : combatants_to_disp)
	{
		if (profelite.count == 0) //shortstop
			break;
		if (kte_settings.bShortNames)
			snprintf(temp, 32, "%02d %s", profelite.count, &pe_short_name_lut[profelite.idx]);
		else
			snprintf(temp, 32, "%02d %s", profelite.count, &pe_name_lut[profelite.idx]);
		draw_bar((float)profelite.count/(float)cur_max, temp, color_array[1][profelite.prof]);
	}

	ImGui::PopStyleColor();
}

void push_new_team_name(char* buf, const uint16_t team_id)
{
	if (kte_settings.blue_team == team_id)
	{
		snprintf(buf, 32, "Blue");
	}
	else if (kte_settings.green_team == team_id)
	{
		snprintf(buf, 32, "Green");
	}
	else if (kte_settings.red_team == team_id)
	{
		snprintf(buf, 32, "Red");
	}
	else if (kte_settings.cteam1.id == team_id)
	{
		snprintf(buf, 16, kte_settings.cteam1.name.data());
	}
	else if (kte_settings.cteam2.id == team_id)
	{
		snprintf(buf, 16, kte_settings.cteam2.name.data());
	}
	else if (kte_settings.cteam3.id == team_id)
	{
		snprintf(buf, 16, kte_settings.cteam3.name.data());
	}
	else
	{
		snprintf(buf, 32, "Team %d", team_id);
	}
}

void draw_teams_tabbed()
{
	ImGuiStyle style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 1));
	ImU32 col = ImGui::GetColorU32(ImGuiCol_TableRowBg);
	ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, col);
	uint8_t cur_max = 0;
	if (override_tab_max_switch == false)
	{
		for (auto& team : team_history_map)
		{
			if (team.second[history_to_disp_idx].total > cur_max)
			{
				cur_max = team.second[history_to_disp_idx].total;
				tab_teamid = team.first;
			}
		}
	}
	if (ImGui::BeginTable("tabtable", 1, 
		ImGuiTableFlags_BordersInner | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoPadOuterX))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		for (auto& team : team_history_map)
		{
			col = ImGui::GetColorU32(ImGuiCol_Tab);
			if (team.first == tab_teamid)
				col = ImGui::GetColorU32(ImGuiCol_TabActive);
			ImGui::PushStyleColor(ImGuiCol_Button, col);
			char temp[32] = {0};
			push_new_team_name(temp, team.first);
			if (ImGui::Button(temp))
			{
				override_tab_max_switch = true;
				tab_teamid = team.first;
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
		}
		ImGui::NewLine();

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		imgui_team_class_bars(team_history_map[tab_teamid][history_to_disp_idx]);
		ImGui::EndTable();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void draw_teams_columns()
{
	ImGuiStyle style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 1));
	ImU32 col = ImGui::GetColorU32(ImGuiCol_Tab);
	ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, col);
	if (ImGui::BeginTable("coltable", team_history_map.size(), 
		ImGuiTableFlags_BordersInner | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoPadOuterX))
	{
		char temp[32] = {0};
		for (auto& team : team_history_map)
		{
			push_new_team_name(temp, team.first);
			ImGui::TableSetupColumn(temp, ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
		}
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
		int column = 0;
		for (auto& team : team_history_map)
		{
			ImGui::TableSetColumnIndex(column);
			const char* column_name = ImGui::TableGetColumnName(column); // Retrieve name passed to TableSetupColumn()
			float inner_pad = ImGui::GetColumnWidth(column)*0.5 - ImGui::CalcTextSize(column_name).x*0.5;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inner_pad);
			ImGui::Text(column_name);
			column++;
		}

		ImGui::TableNextRow();
		column = 0;
		for (auto& team : team_history_map)
		{
			ImGui::TableSetColumnIndex(column);
			imgui_team_class_bars(team_history_map[team.first][history_to_disp_idx]);
			column++;
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void draw_battle_wait()
{
	if (GetTickCount() - last_cbt_evt > 15*1000)
	{
		cmb_state = combat_state::OUT_BATTLE;
	}
	ImGui::NewLine();
	const char* text = "BATTLE IN PROGRESS";
	float inner_pad;
	unsigned int width = ImGui::CalcTextSize(text).x + 4;
	unsigned int win_width = ImGui::GetWindowWidth();
	if (width >= win_width)
	{
		const char* text0 = "BATTLE";
		inner_pad = win_width*0.5 - ImGui::CalcTextSize(text0).x*0.5;
		ImGui::SetCursorPosX(inner_pad);
		ImGui::Text(text0);

		const char* text1 = "IN";
		inner_pad = win_width*0.5 - ImGui::CalcTextSize(text1).x*0.5;
		ImGui::SetCursorPosX(inner_pad);
		ImGui::Text(text1);

		const char* text2 = "PROGRESS";
		inner_pad = win_width*0.5 - ImGui::CalcTextSize(text2).x*0.5;
		ImGui::SetCursorPosX(inner_pad);
		ImGui::Text(text2);

	}
	else
	{
		inner_pad = win_width*0.5 - ImGui::CalcTextSize(text).x*0.5;
		ImGui::SetCursorPosX(inner_pad);
		ImGui::Text(text);
	}
	ImGui::NewLine();
	inner_pad = win_width*0.5 - ImGui::CalcTextSize("00000000").x*0.5;
	ImGui::SetCursorPosX(inner_pad);
	ImGui::Text("%08o", GetTickCount() - logstart_time);
}

display_state get_display_state()
{
	if (team_history_map.empty())
	{
		return display_state::NONE;
	}
	if (cmb_state == combat_state::IN_BATTLE && cur_history_idx == history_to_disp_idx)
	{
		return display_state::BATTLE;
	}
	else if (!kte_settings.bShowColumns)
	{
		return display_state::HISTORY_TAB;
	}
	else if (kte_settings.bShowColumns)
	{
		return display_state::HISTORY_COL;
	}
	return display_state::NONE;
}

uintptr_t imgui_proc(const uint32_t not_charsel_or_loading, const uint32_t hide_if_combat_or_ooc)
{
	if (not_charsel_or_loading && kte_settings.bEnabled && kte_settings.bToShow && 
		(!kte_settings.bHideInCombat || cmb_state == combat_state::OUT_BATTLE))
	{
		bool made_title_invis = false;
		if (!kte_settings.bTitleBg)
		{
			ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg); //xyzw == RGBA
			ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(color.x, color.y, color.z, 0.0)); 
			ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(color.x, color.y, color.z, 0.0)); 
			made_title_invis = true;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(100, 10));

		ImGui::PushID("KTE");
		if (ImGui::Begin("Know thy enemy", &kte_settings.bEnabled, kte_settings.wFlags))
		{
			switch (get_display_state())
			{
			case display_state::BATTLE:
				draw_battle_wait();
				break;
			case display_state::HISTORY_TAB:
				draw_teams_tabbed();
				break;
			case display_state::HISTORY_COL:
				draw_teams_columns();
				break;
			case display_state::NONE:
			default:
				ImGui::Text("No teams");
				ImGui::Separator();
				ImGui::Text("No hits");
				break;
			}

			if(ImGui::BeginPopupContextWindow(NULL, ImGuiPopupFlags_MouseButtonRight))
			{
				ImGuiStyle style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
				if (ImGui::BeginMenu("History"))
				{
					draw_history_menu();
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Style"))
				{
					draw_style_menu();
					ImGui::EndMenu();
				}
				ImGui::PopStyleVar();
				ImGui::EndPopup();
			}
		}
		ImGui::End();
		ImGui::PopID();
		ImGui::PopStyleVar();
		if (made_title_invis)
		{
			ImGui::PopStyleColor(2);
		}
	}
	return 0;
}

void save_kte_settings()
{
	std::wstring path = std::wstring(get_settings_path());
	path = path.substr(0, path.find_last_of(L"\\")+1);
	path.append(L"know_thy_enemy_settings.bin");
	std::fstream file(path.c_str(), std::fstream::out | std::fstream::trunc | std::ios::binary);
	if (file.good())
	{
		file.write((char*)&kte_settings, sizeof(settings));
	}
	file.close();
}

void init_kte_settings()
{
	std::wstring path = std::wstring(get_settings_path());
	path = path.substr(0, path.find_last_of(L"\\")+1);
	path.append(L"know_thy_enemy_settings.bin");
	std::fstream file(path.c_str(), std::fstream::in | std::fstream::out | std::ios::binary);
	std::string line;
	bool success = false;
	if (file.good())
	{
		file.read((char*)&kte_settings, sizeof(settings));
		if(kte_settings.magic == 0xC0FFEE)
			success = true;
	}
	if (!success)
	{
		file.open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::trunc);
	}
	file.close();
}

/* release mod -- return ignored */
uintptr_t mod_release() {
	FreeConsole();
	save_kte_settings();
	return 0;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr() {
	arcvers = 0;
	return mod_release;
}


/* initialize mod -- return table that arcdps will use for callbacks. exports struct and strings are copied to arcdps memory only once at init */
static arcdps_exports arc_exports = {0};
arcdps_exports* mod_init() {
	// if (strcmp(arcvers, "20230822.225438-493-x64") != 0)
	// {
	// 	log_arc("arcdps mismatch, not loading know_thy_enemy");
	// 	return 0;
	// }
	/* for arcdps */
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xC0FFEE;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Know thy enemy";
	arc_exports.out_build = "4.6.3";
	arc_exports.imgui = imgui_proc;
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.options_end = options_end_proc;
	arc_exports.options_windows = options_windows_proc;
	init_colors();
	init_kte_settings();
	cur_history_idx = 0;
	history_to_disp_idx = 0;
	log_arc((char*)"know_thy_enemy mod_init"); // if using vs2015+, project properties > c++ > conformance mode > permissive to avoid const to not const conversion error
	return &arc_exports;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion) {
	// id3dptr is IDirect3D9* if d3dversion==9, or IDXGISwapChain* if d3dversion==11
	arcvers = arcversion;
	get_settings_path = (wchar_t*(*)())GetProcAddress((HMODULE)arcdll, "e0");
	// arccontext_0x508 = (const char*(*)())GetProcAddress((HMODULE)arcdll, "e1");
	// arccontext = arccontext_0x508()-0x508;
	arclog = (size_t(*)(char*))GetProcAddress((HMODULE)arcdll, "e8");
	arccolors = (void(*)(ImVec4**))GetProcAddress((HMODULE)arcdll, "e5");
	get_ui_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e6");
	get_key_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e7");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void *(*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn); // on imgui 1.80+
	return mod_init;
}
