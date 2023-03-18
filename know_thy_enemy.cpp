#include <unordered_map>
#include <Windows.h>
#include <mutex>
#include <algorithm>
#include "imgui/imgui.h"
#include <string>
#include <sstream>
#include <fstream>
#include <array>

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
	Warrior,
	Engineer,
	Ranger,
	Thief,
	Elementalist,
	Mesmer,
	Necromancer,
	Revenant,
	Druid,
	Daredevil,
	Berserker,
	Dragonhunter,
	Reaper,
	Chronomancer,
	Scrapper,
	Tempest,
	Herald,
	Soulbeast,
	Weaver,
	Holosmith,
	Deadeye,
	Mirage,
	Scourge,
	Spellbreaker,
	Firebrand,
	Renegade,
	Harbinger,
	Willbender,
	Virtuoso,
	Catalyst,
	Bladesworn,
	Vindicator,
	Mechanist,
	Specter,
	Untamed,
	Unknown,
	LENGTH
};
}

struct s_profelite {
	uint8_t prof = 0;
	uint8_t elite = 0;
	uint8_t idx = 0;
	uint8_t count = 0;
	char* name = nullptr;
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
				name = "Guardian";
				idx = DATA_ARRAY::Guardian;
				return;
			case 2: 
				name = "Warrior";
				idx = DATA_ARRAY::Warrior;
				return;
			case 3: 
				name = "Engineer";
				idx = DATA_ARRAY::Engineer;
				return;
			case 4: 
				name = "Ranger";
				idx = DATA_ARRAY::Ranger;
				return;
			case 5:  
				name = "Thief";
				idx = DATA_ARRAY::Thief;
				return;
			case 6:  
				name = "Elementalist";
				idx = DATA_ARRAY::Elementalist;
				return;
			case 7:  
				name = "Mesmer";
				idx = DATA_ARRAY::Mesmer;
				return;
			case 8:  
				name = "Necromancer";
				idx = DATA_ARRAY::Necromancer;
				return;
			case 9:  
				name = "Revenant";
				idx = DATA_ARRAY::Revenant;
				return;
			default: 
				name = "Unknown";
				idx = DATA_ARRAY::Unknown;
				return;
			}
		case 5:	
			name = "Druid";
			idx = DATA_ARRAY::Druid;
			return;
		case 7:	
			name = "Daredevil";
			idx = DATA_ARRAY::Daredevil;
			return;
		case 18:
			name = "Berserker";
			idx = DATA_ARRAY::Berserker;
			return;
		case 27:
			name = "Dragonhunter";
			idx = DATA_ARRAY::Dragonhunter;
			return;
		case 34:
			name = "Reaper";
			idx = DATA_ARRAY::Reaper;
			return;
		case 40:
			name = "Chronomancer";
			idx = DATA_ARRAY::Chronomancer;
			return;
		case 43:
			name = "Scrapper";
			idx = DATA_ARRAY::Scrapper;
			return;
		case 48:
			name = "Tempest";
			idx = DATA_ARRAY::Tempest;
			return;
		case 52:
			name = "Herald";
			idx = DATA_ARRAY::Herald;
			return;
		case 55:
			name = "Soulbeast";
			idx = DATA_ARRAY::Soulbeast;
			return;
		case 56:
			name = "Weaver";
			idx = DATA_ARRAY::Weaver;
			return;
		case 57:
			name = "Holosmith";
			idx = DATA_ARRAY::Holosmith;
			return;
		case 58:
			name = "Deadeye";
			idx = DATA_ARRAY::Deadeye;
			return;
		case 59:
			name = "Mirage";
			idx = DATA_ARRAY::Mirage;
			return;
		case 60:
			name = "Scourge";
			idx = DATA_ARRAY::Scourge;
			return;
		case 61:
			name = "Spellbreaker";
			idx = DATA_ARRAY::Spellbreaker;
			return;
		case 62:
			name = "Firebrand";
			idx = DATA_ARRAY::Firebrand;
			return;
		case 63:
			name = "Renegade";
			idx = DATA_ARRAY::Renegade;
			return;
		case 64:
			name = "Harbinger";
			idx = DATA_ARRAY::Harbinger;
			return;
		case 65:
			name = "Willbender";
			idx = DATA_ARRAY::Willbender;
			return;
		case 66:
			name = "Virtuoso";
			idx = DATA_ARRAY::Virtuoso;
			return;
		case 67:
			name = "Catalyst";
			idx = DATA_ARRAY::Catalyst;
			return;
		case 68:
			name = "Bladesworn";
			idx = DATA_ARRAY::Bladesworn;
			return;
		case 69:
			name = "Vindicator";
			idx = DATA_ARRAY::Vindicator;
			return;
		case 70:
			name = "Mechanist";
			idx = DATA_ARRAY::Mechanist;
			return;
		case 71:
			name = "Specter";
			idx = DATA_ARRAY::Specter;
			return;
		case 72:
			name = "Untamed";
			idx = DATA_ARRAY::Untamed;
			return;
		default:
			name = "Unknown";
			idx = DATA_ARRAY::Unknown;
			return;
		}
	}
};

struct s_team_battle {
	uint8_t total = 0;
	uint8_t total_hit = 0;
	std::array<s_profelite, DATA_ARRAY::LENGTH> profelites = std::array<s_profelite, DATA_ARRAY::LENGTH>();
};

std::mutex mtx;

/* proto/globals */
bool enabled = true;
bool toShow = false;
bool bTitleBg = true;

const uint8_t MAX_HISTORY_SIZE = 6;
const uint8_t MAX_STRING_SIZE = 32;
const uint8_t MAX_TOTAL_STRINGS = 64;

// int is_wvw_state = -1;
bool mod_key1 = false;
bool mod_key2 = false;
ImGuiWindowFlags wFlags = 0;


char* arcvers;
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();

/* arcdps exports */
size_t(*arclog)(char*);
void(*arccolors)(ImVec4**);
const char*(*arccontext_0x510)();
wchar_t*(*get_settings_path)();
uint64_t(*get_ui_settings)();
uint64_t(*get_key_settings)();

const char* arccontext = nullptr;


std::unordered_map<uint16_t, bool> ids = std::unordered_map<uint16_t, bool>();
std::unordered_map<uint16_t, std::array<s_team_battle, MAX_HISTORY_SIZE>> team_history_map = std::unordered_map<uint16_t, std::array<s_team_battle, MAX_HISTORY_SIZE>>();

int history_radio_state = 0;
int cur_history_idx = 0;
int history_to_disp_idx = 0;
uint16_t selected_team = 0;

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
			wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	}
	else
		wFlags &= ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	return uMsg;
}

void record_agent(const ag* agent, const uint16_t instid, const uint8_t iHit)
{
	if (agent->team == 0)
		return;

	auto& team = team_history_map[agent->team];

	if (ids.find(instid) != ids.end()) //if found
	{
		if (iHit && ids[instid] == false)
		{
			ids[instid] = true;
			std::lock_guard<std::mutex>lock(mtx);
			team[cur_history_idx].total_hit++;
		}
		return; //dont process found
	}
	else if (iHit) //process unfound
	{
		ids[instid] = true;
		std::lock_guard<std::mutex>lock(mtx);
		team[cur_history_idx].total_hit++;
	}
	else
	{
		ids[instid] = false;
	}

	s_profelite pe(agent->prof & 0xFF, agent->elite & 0xFF);

	std::lock_guard<std::mutex>lock(mtx);
	if (team[cur_history_idx].profelites[pe.idx].name != nullptr)
	{
		team[cur_history_idx].profelites[pe.idx].count++;
	}
	else
	{
		pe.count = 1;
		team[cur_history_idx].profelites[pe.idx] = pe;
	}

	team[cur_history_idx].total++;
	return;
}

bool isWvw()
{
	unsigned short map_id = (arccontext[0x701] << 8) | arccontext[0x700];
	switch ((HackedMapIds)map_id)
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

bool log_ended = false;

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
uintptr_t mod_combat(const cbtevent* ev, const ag* src, const ag* dst, const char* skillname, const uint64_t id, const uint64_t revision) 
{
	if (!ev)
	{
		if (!src->elite) 
		{
			if (src->prof) 
			{
				if (dst->self)
				{
					toShow = isWvw();
				}
			}
		}
	}
	if(enabled && toShow)
	{
		if (ev)
		{
			if (selected_team != 0 && ev->is_statechange == CBTS_LOGEND)
				log_ended = true;
			if (ev->is_activation || ev->is_buffremove || ev->is_statechange || ev->buff || src->elite == 0xFFFFFFFF || dst->elite == 0xFFFFFFFF || src->prof == 0 || dst->prof == 0)
				return 0;
			if (src && dst)
			{
				if (log_ended && (src->name == nullptr || dst->name == nullptr))
				{
					cur_history_idx = (cur_history_idx + 1) % MAX_HISTORY_SIZE;
					std::lock_guard<std::mutex>lock(mtx);
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
						ids.clear();
						history_to_disp_idx = cur_history_idx;
						history_radio_state = 0;
					}
					log_ended = false;
				}

				if (src->name == nullptr)
				{
					record_agent(src, ev->src_instid, 0);
				}
				else if (dst->name == nullptr)
				{
					record_agent(dst, ev->dst_instid, src->self & 1);
				}
			}
		}
	}
	return 0;
}

void options_end_proc(const char* windowname)
{
	ImGui::Checkbox("Know thy enemy##1cb", &enabled);
	ImGui::NewLine();
	ImGui::Separator();
	if (ImGui::Button("Reset settings##fte"))
	{
		enabled = true;
		wFlags = 0;
		bTitleBg = true;
		std::wstring path = std::wstring(get_settings_path());
		path = path.substr(0, path.find_last_of(L"\\")+1);
		path.append(L"know_thy_enemy_settings.txt");
		std::fstream file(path.c_str(), std::fstream::out | std::fstream::trunc);
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


char cstrings[MAX_TOTAL_STRINGS][MAX_STRING_SIZE] = {0}; //64 is good enough
uint8_t cstrings_idx = 0;

void draw_bar(const float frac, const char* text, const ImVec4& color)
{
	ImVec2 upper_left = ImGui::GetCursorScreenPos();
	ImVec2 lower_right = ImVec2(upper_left.x + (ImGui::GetContentRegionAvail().x*frac), upper_left.y + ImGui::GetTextLineHeight() + 2);
	//ImGui::ProgressBar(frac, ImVec2(-1, 0), "");
	ImGui::GetWindowDrawList()->AddRectFilled(upper_left, lower_right, ImGui::ColorConvertFloat4ToU32(color));

	ImVec2 text_start = ImGui::GetCursorPos();
	text_start.y += 1; //middle of bar
	ImGui::SetCursorPos(text_start);
	ImGui::TextUnformatted(text, text + strlen(text));

	text_start.y += ImGui::GetTextLineHeight() + 3; //end of bar + 2 pad
	ImGui::SetCursorPos(text_start);
}

void draw_history_menu()
{
	if(selected_team == 0)
	{
		ImGui::Text("No data...");
	}
	else
	{
		snprintf(&cstrings[cstrings_idx][0], 32, "Current ##fte");
		if (ImGui::RadioButton(&cstrings[cstrings_idx++][0], history_radio_state == 0))
		{
			history_radio_state = 0;
			history_to_disp_idx = cur_history_idx;
			ImGui::CloseCurrentPopup();
		}
		int past_history_idx = (cur_history_idx - 1 + MAX_HISTORY_SIZE) % MAX_HISTORY_SIZE;
		for(int i = 0; i < MAX_HISTORY_SIZE-1; i++)
		{
			snprintf(&cstrings[cstrings_idx][0], 32, "History %d##fte", i+1);
			if(ImGui::RadioButton(&cstrings[cstrings_idx++][0], history_radio_state == (i+1)))
			{
				history_radio_state = i+1;
				history_to_disp_idx = past_history_idx;
				ImGui::CloseCurrentPopup();
			}
			past_history_idx = (past_history_idx - 1 + MAX_HISTORY_SIZE) % MAX_HISTORY_SIZE;
		}
	}
}

void draw_style_menu()
{
	bool bTitlebar = (wFlags & ImGuiWindowFlags_NoTitleBar) == 0;
	if (ImGui::Checkbox("title bar##kte", &bTitlebar))
	{
		wFlags ^= ImGuiWindowFlags_NoTitleBar;
	}
	bool bScrollbar = (wFlags & ImGuiWindowFlags_NoScrollbar) == 0;
	if (ImGui::Checkbox("scroll bar##kte", &bScrollbar))
	{
		wFlags ^= ImGuiWindowFlags_NoScrollbar;
	}
	bool bBackground = (wFlags & ImGuiWindowFlags_NoBackground) == 0;
	if (ImGui::Checkbox("background##kte", &bBackground))
	{
		wFlags ^= ImGuiWindowFlags_NoBackground;
	}
	if (ImGui::Checkbox("title bar background##kte", &bTitleBg))
	{
	}
}

void imgui_team_class_bars()
{
	uint8_t total = 0;
	uint8_t total_hit = 0;
	uint8_t total_disp = 0;
	std::array<s_profelite, DATA_ARRAY::LENGTH> combatants_to_disp = std::array<s_profelite, DATA_ARRAY::LENGTH>();
	{ //sort profelites with counts
		std::lock_guard<std::mutex>lock(mtx);
	 	total = team_history_map[selected_team][history_to_disp_idx].total;
		total_hit = team_history_map[selected_team][history_to_disp_idx].total_hit;
		for(auto profelite : team_history_map[selected_team][history_to_disp_idx].profelites)
		{
			if (profelite.count != 0)
			{
				combatants_to_disp[total_disp] = profelite; 
				total_disp++;
			}
		}
	}
	if (total_disp == 0)
		return;

	std::sort(combatants_to_disp.begin(), combatants_to_disp.begin()+total_disp, [=](s_profelite& a, s_profelite& b)
	{
		return a.count > b.count;
	});

	ImGui::PushStyleColor(ImGuiCol_Text, color_array[0][4]);

	snprintf(&cstrings[cstrings_idx][0], 32, " You hit %d/%d ", total_hit, total);
	draw_bar(1.f, &cstrings[cstrings_idx++][0], ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));

	uint16_t cur_max = combatants_to_disp[0].count;
	for (s_profelite& profelite : combatants_to_disp)
	{
		if (profelite.count == 0) //shortstop
			break;
		snprintf(&cstrings[cstrings_idx][0], 32, " %d %s ", profelite.count, profelite.name);
		draw_bar((float)profelite.count/(float)cur_max, &cstrings[cstrings_idx++][0], color_array[1][profelite.prof]);
	}

	ImGui::PopStyleColor();
}

void push_new_team_name(const uint16_t team_id)
{
	switch (team_id)
	{
	case 432:
		snprintf(&cstrings[cstrings_idx][0], 32, "Blue##fte");
		break;
	case 2739:
		snprintf(&cstrings[cstrings_idx][0], 32, "Green##fte");
		break;
	case 705:
		snprintf(&cstrings[cstrings_idx][0], 32, "Red##fte");
		break;
	default:
		snprintf(&cstrings[cstrings_idx][0], 32, "Team %d##fte", team_id);
		break;
	}
}

uintptr_t imgui_proc(const uint32_t not_charsel_or_loading, const uint32_t hide_if_combat_or_ooc)
{
	if (not_charsel_or_loading && enabled && toShow)
	{
		cstrings_idx = 0;
		bool made_title_invis = false;
		if (!bTitleBg)
		{
			ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg); //xyzw == RGBA
			ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(color.x, color.y, color.z, 0.0)); 
			ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(color.x, color.y, color.z, 0.0)); 
			made_title_invis = true;

		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(100, 10));

		if (ImGui::Begin("Know thy enemy", &enabled, wFlags))
		{
			if (ImGui::BeginTabBar("MyTabBar##fte", 0))
			{
				for (auto& team : team_history_map)
				{
					push_new_team_name(team.first);
					if (ImGui::BeginTabItem(&cstrings[cstrings_idx++][0]))
					{
						selected_team = team.first;
						imgui_team_class_bars();
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}

			if( ImGui::BeginPopupContextWindow(NULL, 1))
			{
				ImGuiStyle style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));
				if (ImGui::BeginMenu("History##fte"))
				{
					draw_history_menu();
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Style##fte"))
				{
					draw_style_menu();
					ImGui::EndMenu();
				}
				ImGui::PopStyleVar();
				ImGui::EndPopup();
			}
		}
		ImGui::End();
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
	path.append(L"know_thy_enemy_settings.txt");
	std::fstream file(path.c_str(), std::fstream::out | std::fstream::trunc);
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
	path = path.substr(0, path.find_last_of(L"\\")+1);
	path.append(L"know_thy_enemy_settings.txt");
	std::fstream file(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
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
	/* for arcdps */
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xC0FFEE;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Know thy enemy";
	arc_exports.out_build = "3.1.1";
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
	arccontext_0x510 = (const char*(*)())GetProcAddress((HMODULE)arcdll, "e1");
	arccontext = arccontext_0x510()-0x510;
	arclog = (size_t(*)(char*))GetProcAddress((HMODULE)arcdll, "e8");
	arccolors = (void(*)(ImVec4**))GetProcAddress((HMODULE)arcdll, "e5");
	get_ui_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e6");
	get_key_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e7");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void *(*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn); // on imgui 1.80+
	return mod_init;
}