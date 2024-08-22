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
#include <vector>
#include <atomic>
#include <time.h>

#include "../include/types.h"
#include "../include/arcdeeps.h"
void* filelog;

const std::array<const char[16], DATA_ARRAY::LENGTH> pe_name_lut = {
	"Unknown",
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
	"Vindicator"
};

const std::array<const char[4], DATA_ARRAY::LENGTH> pe_short_name_lut = {
	"Ukn",
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
	"Vin"
};

std::mutex mtx;

const uint8_t MAX_HISTORY_SIZE = 8; //power of 2

// int is_wvw_state = -1;
bool mod_key1 = false;
bool mod_key2 = false;
uint16_t tab_team_idx = 0;
bool override_tab_max_switch = false;

char* arcvers;
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();

/* arcdps exports */
size_t(*arclog)(char*);
void(*arccolors)(ImVec4**);
wchar_t*(*get_settings_path)();
uint64_t(*get_ui_settings)();
uint64_t(*get_key_settings)();

char* gw2_ctx = nullptr;
char* arcdll = 0;
uint16_t* pNumAgents = 0;
uint16_t* pAgentIds = 0;

struct TeamHistories {
	std::vector<uint16_t> team_ids;
	std::vector<std::array<s_team_battle, MAX_HISTORY_SIZE>> histories;
};


std::vector<uint16_t> ids = std::vector<uint16_t>();
TeamHistories team_history_map = {
	std::vector<uint16_t>(),
	std::vector<std::array<s_team_battle, MAX_HISTORY_SIZE>>()
};
std::array<std::array<char, 32>, MAX_HISTORY_SIZE> history_labels;

settings kte_settings = {};
int kte_loaded = 0;
bool reset_pos = false;

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

/* log to arcdps.log, thread/async safe */
void log_file(char* str) {
	size_t(*log)(char*) = (size_t(*)(char*))filelog;
	if (log) (*log)(str);
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

unsigned int logstart_time = 0xFFFFFFFF;
unsigned int last_cbt_evt = GetTickCount();
combat_state cmb_state = combat_state::OUT_BATTLE;
bool log_start = false;
bool log_end = false;

unsigned short(* get_team_id)(const void* agent) = nullptr;

void setup_new_histories()
{
	bool all_zero = true;
	for (auto& team : team_history_map.histories)
		all_zero = all_zero && team[cur_history_idx].total == 0;
	if (!all_zero)
	{
		cur_history_idx = (cur_history_idx + 1) & (MAX_HISTORY_SIZE - 1);
		for (auto& team : team_history_map.histories)
		{
			if (team[cur_history_idx].total != 0)
			{
				team[cur_history_idx].total = 0;
				for(auto& profelite : team[cur_history_idx].profelites)
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
}

uint16_t my_team_id = 0;
bool try_register_agent(const uint16_t id)
{
	char* cur_agent;
	if (id == 0)
		return false;
	else if (((cur_agent = read_arc_addr_from_gw2(get_agent(gw2_ctx, id))) != 0) && is_valid_foe(cur_agent))
	{
		uint8_t self_maybe = is_self(cur_agent);
		unsigned short instid = get_instid(cur_agent);
		unsigned short team_id = 0;

		team_id = get_team(cur_agent);

		if (team_id == 0 || team_id == 0xFFFF)
			return false;
		if (self_maybe)
			my_team_id = team_id;
		if (team_id == my_team_id)
			return false;

		unsigned short prof = get_prof(cur_agent); //0
		unsigned short elite = get_elite(cur_agent); //2

		if (std::find(ids.begin(), ids.end(), instid) != ids.end()) //if found
		{
			return false; 
		}
		if(log_end)
		{
			log_end = false;
			setup_new_histories();
		}

		std::array<s_team_battle, MAX_HISTORY_SIZE>* team = 0;
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
		{
			if (team_history_map.team_ids[i] == team_id)
			{
				team = &team_history_map.histories[i];
				break;
			}
		}
		if (team == 0)
		{
			team_history_map.team_ids.push_back(team_id);
			team_history_map.histories.push_back(std::array<s_team_battle, MAX_HISTORY_SIZE>());
			team = &team_history_map.histories.back();
		}
		ids.push_back(instid);
		s_team_battle* cur_team = &(*team)[cur_history_idx];

		uint8_t idx = 1 + ((prof & 0xFF) - 1)*sizeof(s_profelite);
		s_profelite* pe = &(cur_team->profelites[idx + 0]);
		pe[0].count += (pe[0].elite == (elite & 0xFF)); //core
		pe[1].count += (pe[1].elite == (elite & 0xFF)); //elite
		pe[2].count += (pe[2].elite == (elite & 0xFF)); //elite
		pe[3].count += (pe[3].elite == (elite & 0xFF)); //elite
		cur_team->total++;
		return true;
	}
	return false;
}

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
uintptr_t mod_combat(const cbtevent* ev, const ag* src, const ag* dst, const char* skillname, const uint64_t id, const uint64_t revision) 
{
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
					kte_settings.bToShow = isWvw(arcdll);
					gw2_ctx = get_arcdeeps((HMODULE)arcdll);
					kte_loaded = gw2_ctx != 0 && kte_settings.bToShow;
					if (!kte_loaded)
						log_arc("maybe? KTE NOT INIT\n");
				}
			}
		}
	}
	else if(ev && kte_settings.bEnabled && kte_settings.bToShow)
	{
		if (ev->is_statechange == CBTS_LOGSTART)
		{
			logstart_time = GetTickCount();
		}
		else if (ev->is_statechange == CBTS_LOGEND)
		{
			cmb_state = combat_state::OUT_BATTLE;
			_strtime_s(&history_labels[cur_history_idx][0], 32);
			snprintf(&history_labels[cur_history_idx][8], 32-9, " (%ds)", (GetTickCount() - logstart_time)/1000);
			log_end = true;
		}
		if (ev->is_activation || ev->is_buffremove || ev->is_statechange || ev->buff || src->elite == 0xFFFFFFFF || dst->elite == 0xFFFFFFFF || src->prof == 0 || dst->prof == 0)
			return 0;
		else if (src && dst && ev->value != 0)
		{
			if (gw2_ctx == 0)
				return 0;

			std::lock_guard<std::mutex>lock(mtx);
			cmb_state = combat_state::IN_BATTLE;
			if (GetTickCount() - last_cbt_evt < 200)
				return 0;

			last_cbt_evt = GetTickCount();

			for (uint16_t i = 0; i < *pNumAgents; i++)
			{
				uint16_t id = pAgentIds[i];
				// snprintf(buf, 256, "n: %d i: %d id: %04X\n", *pNumAgents, i, id);
				// log_file(buf);
				try_register_agent(id);
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
	ImGui::InputText("##cteam1name", kte_settings.cteam1.name.data(), 32);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam1id", (int *)&kte_settings.cteam1.id, 0, 0);

	ImGui::Text("Custom 2 name:");
	ImGui::SameLine();
	ImGui::InputText("##cteam2name", kte_settings.cteam2.name.data(), 32);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam2id", (int *)&kte_settings.cteam2.id, 0, 0);

	ImGui::Text("Custom 3 name:");
	ImGui::SameLine();
	ImGui::InputText("##cteam3name", kte_settings.cteam3.name.data(), 32);
	ImGui::SameLine();
	ImGui::Text("id:");
	ImGui::SameLine();
	ImGui::InputInt("##cteam3id", (int *)&kte_settings.cteam3.id, 0, 0);
	ImGui::NewLine();
	ImGui::Separator();
	if (ImGui::Button("Reset postition"))
	{
		reset_pos = true;
	}
	ImGui::Text("        ");
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
	if(team_history_map.team_ids.size() == 0)
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
	if (kte_settings.cteam1.id == team_id)
	{
		snprintf(buf, 32, kte_settings.cteam1.name.data());
	}
	else if (kte_settings.cteam2.id == team_id)
	{
		snprintf(buf, 32, kte_settings.cteam2.name.data());
	}
	else if (kte_settings.cteam3.id == team_id)
	{
		snprintf(buf, 32, kte_settings.cteam3.name.data());
	}
	else if (kte_settings.blue_team == team_id)
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
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
		{
			if (team_history_map.histories[i][history_to_disp_idx].total > cur_max)
			{
				cur_max = team_history_map.histories[i][history_to_disp_idx].total;
				tab_team_idx = i;
			}
		}
	}
	if (ImGui::BeginTable("tabtable", 1, 
		ImGuiTableFlags_BordersInner | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoPadOuterX))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
		{
			col = ImGui::GetColorU32(ImGuiCol_Tab);
			if (i == tab_team_idx)
				col = ImGui::GetColorU32(ImGuiCol_TabActive);
			ImGui::PushStyleColor(ImGuiCol_Button, col);
			char temp[32] = {0};
			push_new_team_name(temp, team_history_map.team_ids[i]);
			if (ImGui::Button(temp))
			{
				override_tab_max_switch = true;
				tab_team_idx = i;
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
		}
		ImGui::NewLine();

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		imgui_team_class_bars(team_history_map.histories[tab_team_idx][history_to_disp_idx]);
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
	if (ImGui::BeginTable("coltable", team_history_map.team_ids.size(), 
		ImGuiTableFlags_BordersInner | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoPadOuterX))
	{
		char temp[32] = {0};
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
		{
			push_new_team_name(temp, team_history_map.team_ids[i]);
			ImGui::TableSetupColumn(temp, ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
		}
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
		int column = 0;
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
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
		for (int i = 0; i < team_history_map.team_ids.size(); i++)
		{
			ImGui::TableSetColumnIndex(column);
			imgui_team_class_bars(team_history_map.histories[i][history_to_disp_idx]);
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
	if (team_history_map.team_ids.empty())
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


int nimgui_proc = 0;
bool not_init_closing = true;
uintptr_t imgui_proc(const uint32_t not_charsel_or_loading, const uint32_t hide_if_combat_or_ooc)
{
	if (not_init_closing && not_charsel_or_loading && kte_loaded == 0 && nimgui_proc == 600)
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x/4, ImGui::GetIO().DisplaySize.y/2));
		ImGui::SetNextWindowSize(ImVec2(200, 100));
		if (ImGui::Begin("KTE NOT INIT", &not_init_closing))
		{
			ImGui::Text("KTE NOT INIT");
		}
		ImGui::End();
	}
	else if (not_charsel_or_loading && nimgui_proc < 300)
		nimgui_proc++;
	else if (not_charsel_or_loading && kte_settings.bEnabled && kte_settings.bToShow && 
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

		auto style = ImGui::GetStyle();
		if (style.ScrollbarSize == 0)
			kte_settings.wFlags |= ImGuiWindowFlags_NoScrollbar;

		ImGui::PushID("KTE");
		if (reset_pos)
		{
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x/4, ImGui::GetIO().DisplaySize.y/2));
			ImGui::SetNextWindowSize(ImVec2(200, 100));
			reset_pos = false;
		}
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
	std::ifstream file(path.c_str(), std::fstream::in | std::ios::binary);
	std::string line;
	bool success = false;
	if (file.good())
	{
		settings_old temp = {};
		file.read((char*)&temp, sizeof(settings_old));
		if(temp.magic == 0xC0FFEE)
		{
			memcpy(&kte_settings, &temp, sizeof(settings_old) - sizeof(custom_team_old)*3);
			kte_settings.magic = 0xC1FFEE;
			memcpy(&kte_settings.cteam1, &temp.cteam1, sizeof(custom_team_old));
			memcpy(&kte_settings.cteam2, &temp.cteam2, sizeof(custom_team_old));
			memcpy(&kte_settings.cteam3, &temp.cteam3, sizeof(custom_team_old));
			success = true;
		}
		else if(temp.magic == 0xC1FFEE)
		{
			file.seekg(0, std::ios::beg);
			file.read((char*)&kte_settings, sizeof(settings));
			success = true;
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
	arc_exports.out_build = "4.9.13";
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
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE _arcdll, void* mallocfn, void* freefn, uint32_t d3dversion) {
	// id3dptr is IDirect3D9* if d3dversion==9, or IDXGISwapChain* if d3dversion==11
	arcvers = arcversion;
	arcdll = (char*)_arcdll;
	char* arcCtx = get_arc_context(arcdll); //createthread
	pNumAgents = get_p_num_agents(arcCtx);
	pAgentIds = get_p_agents(arcCtx);

	get_settings_path = (wchar_t*(*)())GetProcAddress((HMODULE)arcdll, "e0");
	arclog = (size_t(*)(char*))GetProcAddress((HMODULE)arcdll, "e8");
	filelog = (void*)GetProcAddress((HMODULE)arcdll, "e3");
	arccolors = (void(*)(ImVec4**))GetProcAddress((HMODULE)arcdll, "e5");
	get_ui_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e6");
	get_key_settings = (uint64_t(*)())GetProcAddress((HMODULE)arcdll, "e7");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void *(*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn); // on imgui 1.80+
	return mod_init;
}
