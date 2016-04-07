/**
	Object interaction menu
	Unit tests for the inventory and object actions GUI.
	
	Invokes tests by calling the global function Test*_OnStart(int plr)
	and iterate through all tests.
	The test is completed once Test*_Completed() returns true.
	Then Test*_OnFinished() is called, to be able to reset the scenario
	for the next test.
	
	@author Maikel (test logic), Marky (test)
*/


protected func Initialize()
{
	return;
}

protected func InitializePlayer(int plr)
{
	// Set zoom and move player to the middle of the scenario.
	SetPlayerZoomByViewRange(plr, LandscapeWidth(), nil, PLRZOOM_Direct);
	SetFoW(false, plr);
	GetCrew(plr)->SetPosition(120, 190);
	GetCrew(plr)->MakeInvincible();
	
	// Add test control effect.
	var effect = AddEffect("IntTestControl", nil, 100, 2);
	effect.testnr = 5;
	effect.launched = false;
	effect.plr = plr;
	return true;
}


/*-- Test Control --*/

// Aborts the current test and launches the specified test instead.
global func LaunchTest(int nr)
{
	// Get the control effect.
	var effect = GetEffect("IntTestControl", nil);
	if (!effect)
	{
		// Create a new control effect and launch the test.
		effect = AddEffect("IntTestControl", nil, 100, 2);
		effect.testnr = nr;
		effect.launched = false;
		effect.plr = GetPlayerByIndex(0, C4PT_User);
		return;
	}
	// Finish the currently running test.
	Call(Format("~Test%d_OnFinished", effect.testnr));
	// Start the requested test by just setting the test number and setting 
	// effect.launched to false, effect will handle the rest.
	effect.testnr = nr;
	effect.launched = false;
	return;
}

// Calling this function skips the current test, does not work if last test has been ran already.
global func SkipTest()
{
	// Get the control effect.
	var effect = GetEffect("IntTestControl", nil);
	if (!effect)
		return;
	// Finish the previous test.
	Call(Format("~Test%d_OnFinished", effect.testnr));
	// Start the next test by just increasing the test number and setting 
	// effect.launched to false, effect will handle the rest.
	effect.testnr++;
	effect.launched = false;
	return;
}


/*-- Tests --*/

global func FxIntTestControlStart(object target, proplist effect, int temporary)
{
	if (temporary)
		return FX_OK;
	// Set default interval.
	effect.Interval = 2;
	effect.result = true;
	return FX_OK;
}

global func FxIntTestControlTimer(object target, proplist effect)
{
	// Launch new test if needed.
	if (!effect.launched)
	{
		// Log test start.
		Log("=====================================");
		Log("Test %d started:", effect.testnr);
		// Start the test if available, otherwise finish test sequence.
		if (!Call(Format("~Test%d_OnStart", effect.testnr), effect.plr))
		{
			Log("Test %d not available, this was the last test.", effect.testnr);
			Log("=====================================");
			if (effect.result)
				Log("All tests have passed!");
			else
				Log("At least one test has failed!");
			return -1;
		}
		effect.launched = true;
	}
	else
	{
		effect.launched = false;
		var result = Call(Format("Test%d_Execute", effect.testnr));
		effect.result &= result;
		// Call the test on finished function.
		Call(Format("~Test%d_OnFinished", effect.testnr));
		// Log result and increase test number.
		if (result)
			Log(">> Test %d passed.", effect.testnr);
		else
			Log (">> Test %d failed.", effect.testnr);

		effect.testnr++;
	}
	return FX_OK;
}

// Setups:
// Object with QueryRejectDeparture(source)=true is not transferred
// Object with QueryRejectDeparture(destination)=true is transferred
// Transfer items to the same object

global func Test1_OnStart(int plr){ return true;}
global func Test1_OnFinished(){ return; }
global func Test1_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	var sources = [CreateObjectAbove(Armory, 150, 100),
	               CreateObjectAbove(Clonk, 150, 100),
	               CreateObjectAbove(Lorry, 150, 100)];

	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 50, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);
	

	var passed = true;

	// actual test
	
	Log("Test transfer of simple objects metal and wood from a structure (Armory)");

	for (var source in sources)
	{
		Log("====== Source: %s", source->GetName());
	
		Log("****** Transfer from source to structure");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_structure, container_structure);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromTo, source, container_structure, container_structure);
	
		Log("****** Transfer from source to living (Clonk)");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_living, container_living);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromTo, source, container_living, container_living);
	
		Log("****** Transfer from source to vehicle (Lorry)");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_vehicle, container_vehicle);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromTo, source, container_vehicle, container_vehicle);
	
		Log("****** Transfer from source to surrounding");
		
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_surrounding, nil);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test1_Transfer(menu, menu.TransferObjectsFromTo, source, container_surrounding, nil);
		//}
		if (source) source->RemoveObject();
	}


	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}

global func Test1_Transfer(object menu, tested_function, object source, object destination, object expected_container)
{
	var passed = true;
	
	var wood = source->CreateContents(Wood);
	var metal = source->CreateContents(Metal);
	var to_transfer = [wood, metal];

	menu->Call(tested_function, to_transfer, source, destination);
	
	Log("Transferring from source %v to destination %v", source, destination);

	if (source->GetID() == Clonk
	 && destination->GetID() == Helper_Surrounding
	 && expected_container == nil)
	{
		passed &= doTest("The containing clonk should have a drop command. Got %s, expected %s.", source->GetCommand(0, 0), "Drop");
		passed &= doTest("The containing clonk should have a drop command. Got %s, expected %s.", source->GetCommand(0, 1), "Drop");
	}
	else
	{
		passed &= doTest("Wood is in the destination object. Container is %v, expected %v.", wood->Contained(), expected_container);
		passed &= doTest("Metal is in the destination object. Container is %v, expected %v.", metal->Contained(), expected_container);
	}
	if (wood) wood->RemoveObject();
	if (metal) metal->RemoveObject();
	
	return passed;
}


global func Test2_OnStart(int plr){ return true;}
global func Test2_OnFinished(){ return; }
global func Test2_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	var sources = [CreateObjectAbove(Armory, 150, 100),
	               CreateObjectAbove(Clonk, 150, 100),
	               CreateObjectAbove(Lorry, 150, 100)];

	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 50, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);
	

	var passed = true;

	// actual test
	
	Log("Test transfer of stackable objects arrow and javelin, destination contains 1 item of each");

	for (var source in sources)
	{
		Log("====== Source: %s", source->GetName());
	
		Log("****** Transfer from source to structure");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_structure, container_structure);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromTo, source, container_structure, container_structure);
	
		Log("****** Transfer from source to living (Clonk)");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_living, container_living);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromTo, source, container_living, container_living);
	
		Log("****** Transfer from source to vehicle (Lorry)");
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_vehicle, container_vehicle);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromTo, source, container_vehicle, container_vehicle);
	
		Log("****** Transfer from source to surrounding");
		
		
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_surrounding, nil);
	
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test2_Transfer(menu, menu.TransferObjectsFromTo, source, container_surrounding, nil);

		if (source) source->RemoveObject();
	}


	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}

global func Test2_Transfer(object menu, tested_function, object source, object destination, object expected_container)
{
	var passed = true;
	
	var arrow = source->CreateContents(Arrow);
	var javelin = source->CreateContents(Javelin);
	var to_transfer = [arrow, javelin];
	
	var dest_arrow = destination->CreateContents(Arrow);
	var dest_javelin = destination->CreateContents(Javelin);
	
	dest_arrow->SetStackCount(5);

	menu->Call(tested_function, to_transfer, source, destination);

	Log("Transferring from source %v (%s) to destination %v (%s)", source, source->GetName(), destination, destination->GetName());

	if (source->GetID() == Clonk
	 && destination->GetID() == Helper_Surrounding
	 && expected_container == nil)
	{
		passed &= doTest("The containing clonk should have a drop command. Got %s, expected %s.", source->GetCommand(0, 0), "Drop");
		passed &= doTest("The containing clonk should have a drop command. Got %s, expected %s.", source->GetCommand(0, 1), "Drop");
	}
	else
	{
		passed &= doTest("Arrow is in the destination object. Container is %v, expected %v.", arrow->Contained(), expected_container);
		passed &= doTest("Javelin is in the destination object. Container is %v, expected %v.", javelin->Contained(), expected_container);
	}	
	if (arrow) arrow->RemoveObject();
	if (javelin) javelin->RemoveObject();
	if (dest_arrow) dest_arrow->RemoveObject();
	if (dest_javelin) dest_javelin->RemoveObject();
	
	return passed;
}


global func Test3_OnStart(int plr){ return true;}
global func Test3_OnFinished(){ return; }
global func Test3_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	var sources = [CreateObjectAbove(Armory, 150, 100),
	               CreateObjectAbove(Clonk, 150, 100),
	               CreateObjectAbove(Lorry, 150, 100)];
	
	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 50, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);

	var passed = true;

	// actual test
	
	Log("Test: Transfer 75 single arrows from one container to another, should be merged to 5 stacks of 15 arrows each.");

	for (var source in sources)
	{
		Log("====== Source: %s", source->GetName());

		Log("****** Transfer from source to structure");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_structure, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromTo, source, container_structure, nil);
	
		Log("****** Transfer from source to living (Clonk)");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_living, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromTo, source, container_living, nil);
	
		Log("****** Transfer from source to vehicle (Lorry)");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_vehicle, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromTo, source, container_vehicle, nil);
	
		Log("****** Transfer from source to surrounding");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_surrounding, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test3_Transfer(menu, menu.TransferObjectsFromTo, source, container_surrounding, nil);
		if (source) source->RemoveObject();
	}



	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}

global func Test3_Transfer(object menu, tested_function, object source, object destination, object expected_container)
{
	var passed = true;
	
	var number_stacks = 5;
	var number_arrows = number_stacks * Arrow->MaxStackCount();
	var to_transfer = [];
	for (var i = 0; i < number_arrows; ++i)
	{
		var arrow = source->CreateContents(Arrow);
		PushBack(to_transfer, arrow);
	}
	
	// this has to happen separately, so that the objects do not get stacked when entering the container
	for (var arrow in to_transfer)
	{
		if (arrow) arrow->SetStackCount(1);
	}
	
	menu->Call(tested_function, to_transfer, source, destination);

	Log("Transferring from source %v (%s) to destination %v (%s)", source, source->GetName(), destination, destination->GetName());

	var count_stacks = 0;
	var count_arrows = 0;
	for (var i = 0; i < GetLength(to_transfer); ++i)
	{
		if (to_transfer[i] != nil)
		{
			count_stacks += 1; // count objects that got removed
			count_arrows += to_transfer[i]->GetStackCount();
		}
	}

	passed &= doTest("Number of arrow stack was reduced. Got %d, expected %d.", count_stacks, number_stacks);
	passed &= doTest("Number of individual arrows stayed the same. Got %d, expected %d.", count_arrows, number_arrows);

	return passed;
}


global func Test4_OnStart(int plr){ return true;}
global func Test4_OnFinished(){ return; }
global func Test4_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	var sources = [CreateObjectAbove(Armory, 150, 100),
	               CreateObjectAbove(Clonk, 150, 100),
	               CreateObjectAbove(Lorry, 150, 100)];
	
	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 50, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);

	var passed = true;

	// actual test
	
	Log("Test: Transfer a bow with incomplete arrow stack into a container with another bow and full arrow stack.");

	for (var source in sources)
	{
		Log("====== Source: %s", source->GetName());

		Log("****** Transfer from source to structure");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_structure, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromTo, source, container_structure, nil);
	
		Log("****** Transfer from source to living (Clonk)");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_living, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromTo, source, container_living, nil);
	
		Log("****** Transfer from source to vehicle (Lorry)");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_vehicle, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromTo, source, container_vehicle, nil);
	
		Log("****** Transfer from source to surrounding");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromToSimple, source, container_surrounding, nil);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test4_Transfer(menu, menu.TransferObjectsFromTo, source, container_surrounding, nil);
		if (source) source->RemoveObject();
	}



	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}

global func Test4_Transfer(object menu, tested_function, object source, object destination, object expected_container)
{
	var passed = true;
	
	var destination_bow = destination->CreateContents(Bow);
	var destination_ammo = destination_bow->CreateContents(Arrow);
	var destination_arrow = destination->CreateContents(Arrow);
	
	var source_bow = source->CreateContents(Bow);
	var source_ammo = source->CreateContents(Arrow);
	
	source_ammo->SetStackCount(1);
	destination_ammo->SetStackCount(2);
	destination_arrow->SetStackCount(3);
	
	var to_transfer = [source_bow];
	
	menu->Call(tested_function, to_transfer, source, destination);

	Log("Transferring from source %v (%s) to destination %v (%s)", source, source->GetName(), destination, destination->GetName());

	passed &= doTest("Bow arrow count in source does not change. Got %d, expected %d.", source_ammo->GetStackCount(), 1);
	passed &= doTest("Bow arrow in destination does not change. Got %d, expected %d.", destination_ammo->GetStackCount(), 2);
	passed &= doTest("Arrows in destination do not change. Got %d, expected %d.", destination_arrow->GetStackCount(), 3);

	if (destination_bow) destination_bow->RemoveObject();
	if (destination_ammo) destination_ammo->RemoveObject();
	if (destination_arrow) destination_arrow->RemoveObject();

	if (source_bow) source_bow->RemoveObject();
	if (source_ammo) source_ammo->RemoveObject();

	return passed;
}


global func Test5_OnStart(int plr){ return true;}
global func Test5_OnFinished(){ return; }
global func Test5_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	
	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 150, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);

	var sources = [container_structure,
				   container_living,
				   container_vehicle,
				   container_surrounding];
				   
	var passed = true;

	// actual test
	
	Log("Test: Transfer items to a construction site, only 1 item is needed to complete the site.");

	for (var source in sources)
	{
		Log("====== Source: %s", source->GetName());

		Log("****** Transfer from source to construction site");
	
			Log("*** Function TransferObjectsFromToSimple()");
			passed &= Test5_Transfer(menu, menu.TransferObjectsFromToSimple, source);
			Log("*** Function TransferObjectsFromTo()");
			passed &= Test5_Transfer(menu, menu.TransferObjectsFromTo, source);	
	}

	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}


global func Test5_Transfer(object menu, tested_function, object source)
{
	var passed = true;
	
	var destination = CreateObject(ConstructionSite, 50, 50);
	destination->Set(Sawmill, 0);
	destination->CreateContents(Rock, 4); // this leaves 1 wood remaining
	
	var to_transfer = [source->CreateContents(Wood),
	                   source->CreateContents(Wood),
	                   source->CreateContents(Wood),
	                   source->CreateContents(Metal)];

	Test5_ForceRefresh(source);

	passed &= doTest("The source has the initial wood count. Got %d, expected %d.", source->ContentsCount(Wood), 3);
	passed &= doTest("The source has the initial metal count. Got %d, expected %d.", source->ContentsCount(Metal), 1);

	Log("Transferring from source %v (%s) to destination %v (%s)", source, source->GetName(), destination, destination->GetName());

	menu->Call(tested_function, to_transfer, source, destination);

	//Test5_ForceRefresh(source); This can be done, however it may hide an error

	passed &= doTest("The source has wood remaining. Got %d, expected %d.", source->ContentsCount(Wood), 2);
	passed &= doTest("The source has metal remaining. Got %d, expected %d.", source->ContentsCount(Metal), 1);
	
	if (destination)
	{
		passed &= doTest("The construction site received wood. Got %d, expected %d.", destination->ContentsCount(Wood), 1);
		passed &= doTest("The construction site has the initial rocks. Got %d, expected %d.", destination->ContentsCount(Rock), 4);
	}

	if (destination) destination->RemoveObject();
	
	for (var item in to_transfer) if (item) item->RemoveObject();

	for (var item in FindObjects(Find_Or(Find_ID(Wood), Find_ID(Metal), Find_ID(Rock)))) item->RemoveObject();

	return passed;
}

global func Test5_ForceRefresh(object source)
{
	if (source->GetID() == Helper_Surrounding)
	{
		Log("... contents of surrounding: %v", source.current_objects);
	}
	if (source->GetID() == Helper_Surrounding)
	{
		source.last_search_frame = -1;
		source->~RefreshIfNecessary();	
	}
}





global func TestX_OnStart(int plr){ return true;}
global func TestX_OnFinished(){ return; }
global func TestX_Execute()
{
	// setup
	
	var menu = CreateObject(GUI_ObjectInteractionMenu);

	var source = CreateObject(Dummy);
	
	var container_structure = CreateObjectAbove(Armory, 50, 100);
	var container_living = CreateObjectAbove(Clonk, 50, 100);
	var container_vehicle = CreateObjectAbove(Lorry, 50, 100);
	var container_surrounding = CreateObjectAbove(Helper_Surrounding, 50, 100);
	container_surrounding->InitFor(container_living, menu);

	var passed = true;
	var to_transfer;

	// actual test
	
	Log("Test ");

	Log("****** Transfer from source to structure");
	Log("*** Function TransferObjectsFromToSimple()");
	Log("*** Function TransferObjectsFromTo()");
	Log("****** Transfer from source to living (Clonk)");
	Log("*** Function TransferObjectsFromToSimple()");
	Log("*** Function TransferObjectsFromTo()");
	Log("****** Transfer from source to vehicle (Lorry)");
	Log("*** Function TransferObjectsFromToSimple()");
	Log("*** Function TransferObjectsFromTo()");
	Log("****** Transfer from source to surrounding");
	Log("*** Function TransferObjectsFromToSimple()");
	Log("*** Function TransferObjectsFromTo()");

	if (container_structure) container_structure->RemoveObject();
	if (container_living) container_living->RemoveObject();
	if (container_vehicle) container_vehicle->RemoveObject();
	if (container_surrounding) container_surrounding->RemoveObject();
	if (source) source->RemoveObject();
	if (menu) menu->RemoveObject();
	return passed;
}






global func doTest(description, returned, expected)
{
	var test = (returned == expected);
	
	var predicate = "[Fail]";
	if (test) predicate = "[Pass]";
	
	Log(Format("%s %s", predicate, description), returned, expected);
	return test;
}