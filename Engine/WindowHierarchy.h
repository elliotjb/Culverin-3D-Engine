#pragma once
#ifndef __MODULEHIERARCHY_H__
#define __MODULEHIERARCHY_H__

#include "Module.h"
#include "Globals.h"
#include "ImGui\imgui.h"
#include "WindowManager.h"

class Hierarchy : public WindowManager
{
public:

	Hierarchy();
	virtual ~Hierarchy();

	bool Start();
	update_status Update(float dt);
	void ShowHierarchy();
	bool CleanUp();

private:
	std::string model_name = "";
	bool haveModel = false;

};


#endif //__MODULEHIERARCHY_H__