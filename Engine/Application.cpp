#include "Application.h"
#include "parson.h"
#include "PerfTimer.h"
#include "WindowSceneWorld.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_sdl_gl3.h"
#include "ImGui/ImGuizmo.h"
#include "SDL/include/SDL.h"
#include "mmgr/mmgr.h"

static int malloc_count;
static void *counted_malloc(size_t size);
static void counted_free(void *ptr);

Application::Application()
{
	window = new ModuleWindow();
	input = new ModuleInput();
	audio = new ModuleAudio(true);
	renderer3D = new ModuleRenderer3D();
	camera = new ModuleCamera3D();
	physics = new ModulePhysics3D();
	geometry_manager = new ModuleGeometries();
	scene = new Scene();
	console = new Console();
	gui = new ModuleGUI();
	importer = new ModuleImporter();
	fs = new ModuleFS();
	textures = new ModuleTextures();

	random = new math::LCG();

	// The order of calls is very important!
	// Modules will Init() Start() and Update in this order
	// They will CleanUp() in reverse order

	// Main Modules
	AddModule(window);
	AddModule(fs);
	AddModule(camera);
	AddModule(input);
	AddModule(audio);
	AddModule(physics);
	AddModule(geometry_manager);
	AddModule(console);
	AddModule(scene);
	AddModule(gui);
	AddModule(importer);
	AddModule(textures);

	// Renderer last!
	AddModule(renderer3D);
}

Application::~Application()
{
	p2List_item<Module*>* item = list_modules.getLast();

	while(item != NULL)
	{
		delete item->data;
		item = item->prev;
	}
	RELEASE(random);
}

bool Application::Init()
{
	bool ret = true;

	JSON_Value* config_file;
	JSON_Object* config;
	JSON_Object* config_node;

	config_file = json_parse_file("config.json");

	configuration = new DockContext();
	
	//In case of error
	if (config_file != nullptr)
	{
		ret = true;

		config = json_value_get_object(config_file);
		config_node = json_object_get_object(config,"Application");
		appName = json_object_get_string(config_node, "App Name");
		orgName = json_object_get_string(config_node, "Org Name");
		maxFPS = json_object_get_number(config_node, "Max FPS");
		vsync = json_object_get_boolean(config_node, "VSYNC");
		SetFpsCap(maxFPS);
		//------------------------------------

		// Call Init() in all modules
		p2List_item<Module*>* item = list_modules.getFirst();

		while (item != NULL && ret == true)
		{
			if (item->data->IsEnabled())
			{
				config_node = json_object_get_object(config, item->data->name.c_str());
				ret = item->data->Init(config_node);
			}
			item = item->next;
		}

		// After all Init calls we call Start() in all modules
		LOG("Application Start --------------");
		item = list_modules.getFirst();

		while (item != NULL && ret == true)
		{
			if (item->data->IsEnabled())
				ret = item->data->Start();
			item = item->next;
		}
		realTime.engineStart_time.Start();
		realTime.ms_timer.Start();

	}
	return ret;
}

// ---------------------------------------------
void Application::PrepareUpdate()
{
	realTime.frame_count++;
	realTime.last_sec_frame_count++;
	realTime.dt = (float)realTime.ms_timer.ReadSec();
	realTime.ms_timer.Start();
	realTime.frame_time.Start();

	if (gameTime.prepare_frame)
	{
		gameTime.play_frame = true;
		gameTime.prepare_frame = false;
	}

	if (engineState == EngineState::PLAY || engineState == EngineState::PLAYFRAME)
	{
		gameTime.gameStart_time += realTime.dt * gameTime.timeScale;
		gameTime.frame_count++;
	}

	if (change_to_game)
	{
		renderer3D->SetActiveCamera(renderer3D->game_camera);
		change_to_game = false;
	}
	else if (change_to_scene)
	{
		renderer3D->SetActiveCamera(renderer3D->scene_camera);
		change_to_scene = false;
	}
}

// ---------------------------------------------
void Application::FinishUpdate()
{
	// SAVE & LOAD FUNCTIONS ------------------------
	if (want_to_save == true)
	{
		App->scene->SaveScene();
		want_to_save = false;
	}

	if (want_to_load == true)
	{
		App->scene->DeleteGameObjects(App->scene->gameobjects);
		App->scene->LoadScene();

		want_to_load = false;
	}
	// ---------------------------------------------

	// Framerate calculations ----------------------
	if (realTime.last_sec_frame_time.Read() > 1000)
	{
		realTime.last_sec_frame_time.Start();
		realTime.prev_last_sec_frame_count = realTime.last_sec_frame_count;

		fps_log[frame_index] = realTime.prev_last_sec_frame_count;
		frame_index = (frame_index + 1) % IM_ARRAYSIZE(fps_log);

		realTime.last_sec_frame_count = 0;
	}

	float avg_fps = float(realTime.frame_count) / realTime.engineStart_time.ReadSec();
	float seconds_since_startup = realTime.engineStart_time.ReadSec();
	realTime.last_frame_ms = realTime.frame_time.Read();
	uint32 frames_on_last_update = realTime.prev_last_sec_frame_count;
	
	ms_log[ms_index] = realTime.last_frame_ms;

	//Get all performance data-------------------
	p2List_item<Module*>* item = list_modules.getFirst();

	while (item != NULL)
	{
		if (item->data->IsEnabled())
		{
			item->data->pre_log[ms_index] = item->data->preUpdate_t;
			item->data->up_log[ms_index] = item->data->Update_t;
			item->data->post_log[ms_index] = item->data->postUpdate_t;
		}
		item = item->next;
	}

	ms_index = (ms_index + 1) % IM_ARRAYSIZE(ms_log); //ms_index works for all the logs (same size)


	if (realTime.capped_ms > 0 && realTime.last_frame_ms < realTime.capped_ms)
	{
		SDL_Delay(realTime.capped_ms - realTime.last_frame_ms);
	}

	if (gameTime.play_frame)
	{
		gameTime.play_frame = false;
		SetState(EngineState::PAUSE);
	}
}

// Call PreUpdate, Update and PostUpdate on all modules
update_status Application::Update()
{
	update_status ret = UPDATE_CONTINUE;
	PrepareUpdate();

	// SAVE / LOAD  keys ---------------------------------------
	if (App->input->GetKey(SDL_SCANCODE_Z) == KEY_DOWN)
	{
		want_to_save = true;
	}
	if (App->input->GetKey(SDL_SCANCODE_X) == KEY_DOWN)
	{
		want_to_load = true;
	}
	// ---------------------------------------------------------
	
	p2List_item<Module*>* item = list_modules.getFirst();
	while(item != NULL && ret == UPDATE_CONTINUE)
	{
		if (item->data->IsEnabled())
		{
			if (item->data == camera)
			{
				ret = item->data->PreUpdate(realTime.dt); // Camera can't be affected by Game Time Scale (0 dt = 0 movement)
			}
			else
			{
				if (engineState == EngineState::PLAY || engineState == EngineState::PLAYFRAME)
				{
					ret = item->data->PreUpdate(realTime.dt * gameTime.timeScale);
				}
				else if (engineState == EngineState::PAUSE || engineState == EngineState::STOP)
				{
					ret = item->data->PreUpdate(0);
				}
			}
		}
		item = item->next;
	}

	ImGui_ImplSdlGL3_NewFrame(window->window);

	// GIZMO BEGIN FRAME: you have to get the parameters of scene window (x,y,w,h)
	((SceneWorld*)App->gui->winManager[SCENEWORLD])->GetWindowParams(SceneDock.x, SceneDock.y, SceneDock.z, SceneDock.w);
	ImGuizmo::BeginFrame(SceneDock.x, SceneDock.y, SceneDock.z, SceneDock.w);

	item = list_modules.getFirst();

	while(item != NULL && ret == UPDATE_CONTINUE)
	{
		if (item->data->IsEnabled())
		{
			if (item->data == camera)
			{
				ret = item->data->Update(realTime.dt); // Camera can't be affected by Game Time Scale (0 dt = 0 movement)
			}
			else
			{
				if (engineState == EngineState::PLAY || engineState == EngineState::PLAYFRAME)
				{
					ret = item->data->Update(realTime.dt * gameTime.timeScale);
				}
				else if (engineState == EngineState::PAUSE || engineState == EngineState::STOP)
				{
					ret = item->data->Update(0);
				}
			}
		}
		item = item->next;
	}

	item = list_modules.getFirst();
	while (item != NULL && ret == UPDATE_CONTINUE)
	{
		if (item->data->IsEnabled())
		{
			if (item->data == camera)
			{
				ret = item->data->PostUpdate(realTime.dt); // Camera can't be affected by Game Time Scale (0 dt = 0 movement)
			}
			else
			{
				if (engineState == EngineState::PLAY || engineState == EngineState::PLAYFRAME)
				{
					ret = item->data->PostUpdate(realTime.dt * gameTime.timeScale);
				}
				else if (engineState == EngineState::PAUSE || engineState == EngineState::STOP)
				{
					ret = item->data->PostUpdate(0);
				}
			}
		}
		item = item->next;
	}

	//CONFIG WINDOW ----------------------------
	//Config();

	//PERFORMANCE WINDOW -----------------------
	if (showperformance)
	{
		static bool stop_perf = false;
		item = list_modules.getFirst();

		if (!ImGui::Begin("PERFORMANCE", &showperformance, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			stop_perf = true;
		}
		else if (stop_perf == false)
		{
			ImGui::Spacing();
			while (item != NULL)
			{
				if (item->data->IsEnabled())
				{
					item->data->ShowPerformance(ms_index);
				}
				item = item->next;
			}
			ImGui::End();
		}
		stop_perf = false;
	}
	//-----------------------------------------------

	FinishUpdate();
	return ret;
}

void Application::Config()
{
	if (showconfig)
	{
		p2List_item<Module*>* item = list_modules.getFirst();
		static bool stop_conf = false;
		bool ret = true;
		item = list_modules.getFirst();

		if (!BeginDock("CONFIGURATION", &showconfig, 0))
		{
			EndDock();
			stop_conf = true;
		}
		configuration->_BeginWorkspace("ConfigurationWindow");
		if (stop_conf == false)
		{
			//ImGui::Spacing();
			//ImGui::Begin
			static bool* temp = NULL;

			if (!configuration->_BeginDock("Application", temp, 0))
			{
				configuration->_EndDock();
			}
			else
			{
				ImGui::Text("App Name:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), appName.c_str());
				ImGui::Text("Organization Name:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), orgName.c_str());
				static int fps = maxFPS;
				ImGui::SliderInt("Max FPS", &fps, 0, 60);
				ImGui::SameLine(); ShowHelpMarker("0 = no framerate cap"); ImGui::SameLine();
				if (ImGui::Button("APPLY"))
				{
					SetFpsCap(fps);
				}
				ImGui::Text("Framerate:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.0f", fps_log[frame_index - 1]);
				ImGui::PlotHistogram("", fps_log, IM_ARRAYSIZE(fps_log), 0, NULL, 0.0f, 120.0f, ImVec2(0, 80));
				ImGui::Text("Milliseconds:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.0f", ms_log[ms_index - 1]);
				ImGui::PlotHistogram("", ms_log, IM_ARRAYSIZE(ms_log), 0, NULL, 0.0f, 50.0f, ImVec2(0, 80));
				ImGui::Checkbox("VSYNC", &vsync); ImGui::SameLine();
				ShowHelpMarker("Restart to apply changes");

				// TIME MANAGEMENT --------------------------------------
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
				if (ImGui::TreeNodeEx("TIME MANAGEMENT", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PopStyleColor();

					ImGui::Text("Time Since Startup:"); ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.0f s", realTime.engineStart_time.ReadSec());
					ImGui::Text("Frames in Last Second:"); ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%u", realTime.prev_last_sec_frame_count);
					
					ImGui::Spacing();

					ImGui::TextColored(ImVec4(1.0f,1.0f,0.0f,1.0f), "GAME CLOCK");
					ImGui::SliderFloat("Time Scale", &gameTime.timeScale, 0.0f, 5.0f);
					ImGui::Text("Time Since Game Started:"); ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.2f s", gameTime.gameStart_time);
					ImGui::Text("Total Frames:"); ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%u", gameTime.frame_count);

					ImGui::TreePop();
				}
				else
				{
					ImGui::PopStyleColor();
				}
				// ------------------------------------------------------

				configuration->_EndDock();
			}
			sMStats stats = m_getMemoryStatistics();
			if (!configuration->_BeginDock("Memory Consumption", temp, 0))
			{
				configuration->_EndDock();
			}
			else
			{
				ImGui::BulletText("ACCUMULATED");
				ImGui::Text("- Actual Memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.accumulatedActualMemory);
				ImGui::Text("- Allocated memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.accumulatedAllocUnitCount);
				ImGui::Text("- Reported memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.accumulatedReportedMemory);
				ImGui::Spacing();

				ImGui::BulletText("PEAK");
				ImGui::Text("- Actual Memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.peakActualMemory);
				ImGui::Text("- Allocated memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.peakAllocUnitCount);
				ImGui::Text("- Reported memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.peakReportedMemory);
				ImGui::Spacing();

				ImGui::BulletText("TOTAL");
				ImGui::Text("- Actual memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.totalActualMemory);
				ImGui::Text("- Allocated memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.totalAllocUnitCount);
				ImGui::Text("- Reported memory:"); ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.08f, 1.0f), "%i", stats.totalReportedMemory);

				configuration->_EndDock();
			}

			while (item != NULL && ret == UPDATE_CONTINUE)
			{
				if (item->data->IsEnabled() && item->data->haveConfig)
				{
					static bool* closeButton = NULL;
					if (!configuration->_BeginDock(item->data->name.c_str(), closeButton, ImGuiWindowFlags_NoCollapse))
					{
						configuration->_EndDock();
					}
					else
					{
						ret = item->data->UpdateConfig(realTime.dt);
						configuration->_EndDock();
					}
				}

				item = item->next;
			}
			configuration->_EndWorkspace();
			EndDock();
		}
		else
		{
			configuration->_EndWorkspace();
		}
		stop_conf = false;
	}
}

bool Application::CleanUp()
{
	bool ret = true;

	//Before Cleaning, save data to config file
	SaveConfig();

	p2List_item<Module*>* item = list_modules.getLast();

	while(item != NULL && ret == true)
	{
		if (item->data->IsEnabled())
		{
			ret = item->data->CleanUp();
		}
		item = item->prev;
	}
	return ret;
}

bool Application::GetVSYNC()
{
	return vsync;
}

void Application::SetFpsCap(uint fps)
{
	maxFPS = fps;

	if (fps > 0)
	{
		realTime.capped_ms = 1000 / fps;
	}
	else
	{
		realTime.capped_ms = 0;
	}

}

bool Application::SaveConfig()
{
	bool ret = false;

	LOG("SAVING CONFIG TO FILE -----------------------")

	JSON_Value* config_file;
	JSON_Object* config;
	JSON_Object* config_node;

	config_file = json_parse_file("config.json");

	if (config_file != nullptr)
	{
		ret = true;

		config = json_value_get_object(config_file);
		config_node = json_object_get_object(config, "Application");

		json_object_set_string(config_node, "App Name", appName.c_str());
		json_object_set_string(config_node, "Org Name", orgName.c_str());
		json_object_set_number(config_node, "Max FPS", maxFPS);
		json_object_set_boolean(config_node, "VSYNC", vsync);


		//Iterate all modules to save each respective info
		p2List_item<Module*>* item = list_modules.getFirst();

		while (item != NULL && ret == true)
		{
			config_node = json_object_get_object(config, item->data->name.c_str());
			ret = item->data->SaveConfig(config_node);
			item = item->next;
		}

		json_serialize_to_file(config_file, "config.json");
	}


	return ret;
}

void Application::ShowHelpMarker(const char * desc, const char * icon)
{
	ImGui::TextDisabled(icon);
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(450.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

char* Application::GetCharfromConstChar(const char* name)
{
	int length = strlen(name);
	char* temp = new char[length + 1];
	strcpy(temp, name);
	temp[length] = '\0';
	return temp;
}

void Application::SetState(EngineState state)
{
	if (state == EngineState::PLAY)
	{
		if (engineState == EngineState::PLAY)
		{
			engineState = EngineState::STOP;
			gameTime.gameStart_time = 0.0f;
			gameTime.frame_count = 0.0f;
			ChangeCamera("Scene");
			WantToLoad();
		}
		else
		{
			engineState = EngineState::PLAY;
			ChangeCamera("Game");
			WantToSave();
		}
	}
	else 
	{
		engineState = state;
	}
	LOG("Engine State is Now: %i", engineState);
}

void Application::WantToSave()
{
	want_to_save = true;
}

void Application::WantToLoad()
{
	want_to_load = true;
}

void Application::ChangeCamera(const char* window)
{
	if (window == "Game")
	{
		change_to_game = true;
		change_to_scene = false;
	}
	else if (window == "Scene")
	{
		change_to_game = false;
		change_to_scene = true;
	}
}



void Application::AddModule(Module* mod)
{
	list_modules.add(mod);
}

static void *counted_malloc(size_t size) {
	void *res = malloc(size);
	if (res != NULL) {
		malloc_count++;
	}
	return res;
}

static void counted_free(void *ptr) {
	if (ptr != NULL) {
		malloc_count--;
	}
	free(ptr);
}