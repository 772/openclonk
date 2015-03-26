/*
	Standard clonk controls
	Author: Newton
	
	This object provides handling of the clonk controls including item
	management, backpack controls and standard throwing behaviour. It
	should be included into any clonk/crew definition.
	The controls in System.ocg/PlayerControl.c only provide basic movement
	handling, namely the movement left, right, up and down. The rest is
	handled here:
	Grabbing, ungrabbing, shifting and pushing vehicles into buildings;
	entering and exiting buildings; throwing, dropping; backpack control,
	(object) menu control, hotkey controls, usage and it's callbacks and
	forwards to script.
	
	Objects that inherit this object need to return _inherited() in the
	following callbacks (if defined):
		Construction, Collection2, Ejection, RejectCollect, Departure,
		Entrance, AttachTargetLost, CrewSelection, Death,
		Destruction, OnActionChanged
	
	The following callbacks are made to other objects:
		*Stop
		*Left, *Right, *Up, *Down
		*Use, *UseStop, *UseStart, *UseHolding, *UseCancel
	wheras * is 'Contained' if the clonk is contained and otherwise (riding,
	pushing, to self) it is 'Control'. The item in the inventory only gets
	the Use*-calls. If the callback is handled, you should return true.
	Currently, this is explained more in detail here:
	http://forum.openclonk.org/topic_show.pl?tid=337
*/

// make use of other sub-libraries
#include Library_Inventory
#include Library_ClonkInventoryControl
#include Library_ClonkGamepadControl

// used for interaction with objects
static const ACTIONTYPE_INVENTORY = 0;
static const ACTIONTYPE_VEHICLE = 1;
static const ACTIONTYPE_STRUCTURE = 2;
static const ACTIONTYPE_SCRIPT = 3;
static const ACTIONTYPE_EXTRA = 4;

/* ++++++++++++++++++++++++ Clonk Inventory Control ++++++++++++++++++++++++ */

/*
	used properties
	this.control.hotkeypressed: used to determine if an interaction has already been handled by a hotkey (space + 1-9)
	
	this.control.current_object: object that is being used at the moment
	this.control.using_type: way of usage
	this.control.mlastx: last x position of the cursor
	this.control.mlasty: last y position of the cursor
	this.control.noholdingcallbacks: whether to do HoldingUseControl callbacks
	this.control.shelved_command: command (function) with condition that will be executed when the condition is met
		used for example to re-call *Use/Throw commands when the Clonk finished scaling
*/


/* Item limit */
public func MaxContentsCount() { return 5; }		// Size of the clonks inventory
public func HandObjects() { return 1; } // Amount of hands to select items
public func NoStackedContentMenu() { return true; }	// Contents-Menu shall display each object in a seperate slot


/* ################################################# */

protected func Construction()
{
	if(this.control == nil)
		this.control = {};
	this.control.hotkeypressed = false;
	
	menu = nil;

	this.control.current_object = nil;
	this.control.using_type = nil;
	this.control.shelved_command = nil;
	
	return _inherited(...);
}

public func GetUsedObject() { return this.control.current_object; }

// The using-command hast to be canceled if the clonk is entered into
// or exited from a building.

protected func Entrance()         { CancelUse(); return _inherited(...); }
protected func Departure()        { CancelUse(); return _inherited(...); }

// The same for vehicles
protected func AttachTargetLost() { CancelUse(); return _inherited(...); }

// ...aaand the same for when the clonk is deselected
protected func CrewSelection(bool unselect)
{
	if (unselect)
	{
		// cancel usage on unselect first...
		CancelUse();
		// and if there is still a menu, cancel it too...
		CancelMenu();
	}
	return _inherited(unselect,...);
}

protected func Destruction()
{
	// close open menus, cancel usage...
	CancelUse();
	CancelMenu();
	return _inherited(...);
}

protected func Death()
{
	// close open menus, cancel usage...
	CancelUse();
	CancelMenu();
	return _inherited(...);
}

protected func OnActionChanged(string oldaction)
{
	var old_act = this["ActMap"][oldaction];
	var act = this["ActMap"][GetAction()];
	var old_proc = 0;
	if(old_act) old_proc = old_act["Procedure"];
	var proc = 0;
	if(act) proc = act["Procedure"];
	// if the object's procedure has changed from a non Push/Attach
	// to a Push/Attach action or the other way round, the usage needs
	// to be cancelled
	if (proc != old_proc)
	{
		if (proc == DFA_PUSH || proc == DFA_ATTACH
		 || old_proc == DFA_PUSH || old_proc == DFA_ATTACH)
		{
			CancelUse();
		}
	}
	return _inherited(oldaction,...);
}

/** Returns additional interactions the clonk possesses as an array of function pointers.
	Returned Proplist contains:
		Fn			= Name of the function to call
		Object		= Object to call the function in. Will also be displayed on the interaction-button
		Description	= A description of what the interaction does
		IconID		= ID of the definition that contains the icon (like GetInteractionMetaInfo)
		IconName	= Name of the graphic for the icon (like GetInteractionMetaInfo)
		Priority	= Where to sort in in the interaction-list. 0=front, 10=after script, 20=after vehicles, 30=after structures, nil means no preverence
*/
public func GetExtraInteractions()
{
	var functions = _inherited(...) ?? [];
	
	// flipping construction-preview
	var effect;
	if(effect = GetEffect("ControlConstructionPreview", this))
	{
		if(effect.flipable)
			PushBack(functions, {Fn = "Flip", Description=ConstructionPreviewer->GetFlipDescription(), Object=effect.preview, IconID=ConstructionPreviewer_IconFlip, Priority=0});
	}
		
	return functions;
}

/* +++++++++++++++++++++++++++ Clonk Control +++++++++++++++++++++++++++ */

/* Main control function */
public func ObjectControl(int plr, int ctrl, int x, int y, int strength, bool repeat, bool release)
{
	if (!this) 
		return false;

	// some controls should only do something on release (everything that has to do with interaction)
	if(ctrl == CON_Interact)
	{
		if (!release)
		{
						
			if(GetMenu())
			{
				// close a possible menu but still open the action bar later
				TryCancelMenu();
				// interaction with a menu counts as a special hotkey
				this.control.hotkeypressed = true;
				return true;
			}
			
			// this is needed to reset the hotkey-memory
			this.control.hotkeypressed = false;
			
			this->~StartInteractionCheck(this); // for GUI_Controller_ActionBar
			return true;
		}
		// if the interaction-command has already been handled by a hotkey (else it'd double-interact)
		else if(this.control.hotkeypressed)
			return true;
		// check if we can handle it by simply accessing the first actionbar item (for consistency)
		else
		{
			var closed = this->~StopInteractionCheck(); // for GUI_Controller_ActionBar
						
			// releasing of space cancels the action bar without selecting anything
			if (closed)
				return true;
			
			// if the first actionbar item can not be handled, look for interaction objects and use the one with the best priority
			var interaction_objects = GetInteractableObjects();
			// look for minimum priority
			var best = nil;
			for (var info in interaction_objects)
				if (best == nil || (info.priority < best.priority)) best = info;
			if (best)
			{
				ExecuteInteraction(best);
				return true;
			}
		}
	}
	
	// Contents menu
	if (ctrl == CON_Contents && !release)
	{
		// Close any menu if open.
		if (GetMenu())
		{
			var is_content = GetMenu()->~IsContentMenu();
			// unclosable menu? bad luck
			if (!TryCancelMenu()) return true;
			// If contents menu, don't open new one and return.
			if (is_content)
				return true;
		}
		// Open contents menu.
		CancelUse();
		GUI_ObjectInteractionMenu->CreateFor(this);
		// the interaction menu calls SetMenu(this) in the clonk
		// so after this call menu = the created menu
		if(GetMenu())
			GetMenu()->~Show();		
		return true;
	}
	
	/* aiming with mouse:
	   The CON_Aim control is transformed into a use command. Con_Use if
	   repeated does not bear the updated x,y coordinates, that's why this
	   other control needs to be issued and transformed. CON_Aim is a
	   control which is issued on mouse move but disabled when not aiming
	   or when HoldingEnabled() of the used item does not return true.
	   For the rest of the control code, it looks like the x,y coordinates
	   came from CON_Use.
	  */
	if (this.control.current_object && ctrl == CON_Aim)
	{
		ctrl = CON_Use;
				
		repeat = true;
		release = false;
	}
	// controls except a few reset a previously given command
	else SetCommand("None");
	
	/* aiming with analog pad or keys:
	   This works completely different. There are CON_AimAxis* and CON_Aim*,
	   both work about the same. A virtual cursor is created which moves in a
	   circle around the clonk and is controlled via these CON_Aim* functions.
	   CON_Aim* is normally on the same buttons as the movement and has a
	   higher priority, thus is called first. The aim is always done, even if
	   the clonk is not aiming. However this returns only true (=handled) if
	   the clonk is really aiming. So if the clonk is not aiming, the virtual
	   cursor aims into the direction in which the clonk is running and e.g.
	   CON_Left is still called afterwards. So if the clonk finally starts to
	   aim, the virtual cursor already aims into the direction in which he ran
	*/
	if (ctrl == CON_AimAxisUp || ctrl == CON_AimAxisDown || ctrl == CON_AimAxisLeft || ctrl == CON_AimAxisRight
	 || ctrl == CON_AimUp     || ctrl == CON_AimDown     || ctrl == CON_AimLeft     || ctrl == CON_AimRight)
	{
		var success = VirtualCursor()->Aim(ctrl,this,strength,repeat,release);
		// in any case, CON_Aim* is called but it is only successful if the virtual cursor is aiming
		return success && VirtualCursor()->IsAiming();
	}
	
	// save last mouse position:
	// if the using has to be canceled, no information about the current x,y
	// is available. Thus, the last x,y position needs to be saved
	if (ctrl == CON_Use)
	{
		this.control.mlastx = x;
		this.control.mlasty = y;
	}
		
	var proc = GetProcedure();
	
	// building, vehicle, mount, contents, menu control
	var house = Contained();
	var vehicle = GetActionTarget();
	// the clonk can have an action target even though he lost his action. 
	// That's why the clonk may only interact with a vehicle if in an
	// appropiate procedure:
	if (proc != "ATTACH" && proc != "PUSH")
		vehicle = nil;
	
	// menu
	if (menu)
	{
		return Control2Menu(ctrl, x,y,strength, repeat, release);
	}
	
	var contents = this->GetHandItem(0);	
	
	// usage
	var use = (ctrl == CON_Use || ctrl == CON_UseDelayed);
	if (use)
	{
		if (house)
		{
			return ControlUse2Script(ctrl, x, y, strength, repeat, release, house);
		}
		// control to grabbed vehicle
		else if (vehicle && proc == "PUSH")
		{
			return ControlUse2Script(ctrl, x, y, strength, repeat, release, vehicle);
		}
		else if (vehicle && proc == "ATTACH")
		{
			/* objects to which clonks are attached (like horses, mechs,...) have
			   a special handling:
			   Use controls are, forwarded to the
			   horse but if the control is considered unhandled (return false) on
			   the start of the usage, the control is forwarded further to the
			   item. If the item then returns true on the call, that item is
			   regarded as the used item for the subsequent ControlUse* calls.
			   BUT the horse always gets the ControlUse*-calls that'd go to the used
			   item, too and before it so it can decide at any time to cancel its
			   usage via CancelUse().
			  */

			if (ControlUse2Script(ctrl, x, y, strength, repeat, release, vehicle))
				return true;
			else
			{
				// handled if the horse is the used object
				// ("using" is set to the object in StartUse(Delayed)Control - when the
				// object returns true on that callback. Exactly what we want)
				if (this.control.current_object == vehicle) return true;
				// has been cancelled (it is not the start of the usage but no object is used)
				if (!this.control.current_object && (repeat || release)) return true;
			}
		}
		// releasing the use-key always cancels shelved commands (in that case no this.control.current_object exists)
		if(release) StopShelvedCommand();
		// Release commands are always forwarded even if contents is 0, in case we
		// need to cancel use of an object that left inventory
		if (contents || (release && this.control.current_object))
		{
			if (ControlUse2Script(ctrl, x, y, strength, repeat, release, contents))
				return true;
		}
	}
	
	// A click on throw can also just abort usage without having any other effects.
	// todo: figure out if wise.
	var currently_in_use = this.control.current_object != nil;
	if ((ctrl == CON_Throw || ctrl == CON_ThrowDelayed) && currently_in_use && !release)
	{
		CancelUse();
		return true;
	}
	
	// Throwing and dropping
	// only if not in house, not grabbing a vehicle and an item selected
	// only act on press, not release
	if (!house && (!vehicle || proc == "ATTACH") && !release)
	{		
		if (contents && !contents->~QueryRejectDeparture(this))
		{		
			// just drop in certain situations
			var only_drop = proc == "SCALE" || proc == "HANGLE" || proc == "SWIM";
			// also drop if no throw would be possible anyway
			if (only_drop || Distance(0, 0, x, y) < 10 || (Abs(x) < 10 && y > 10))
				only_drop = true;
			// throw
			if (ctrl == CON_Throw)
			{
				CancelUse();
				
				if (only_drop)
					return ObjectCommand("Drop", contents);
				else
					return ObjectCommand("Throw", contents, x, y);
			}
			// throw delayed
			if (ctrl == CON_ThrowDelayed)
			{
				CancelUse();
				if (release)
				{
					VirtualCursor()->StopAim();
				
					if (only_drop)
						return ObjectCommand("Drop", contents);
					else
						return ObjectCommand("Throw", contents, this.control.mlastx, this.control.mlasty);
				}
				else
				{
					VirtualCursor()->StartAim(this);
					return true;
				}
			}
			// drop
			if (ctrl == CON_Drop)
			{
				CancelUse();
				return ObjectCommand("Drop", contents);
			}
		}
	}
	
	// Movement controls (defined in PlayerControl.c, partly overloaded here)
	if (ctrl == CON_Left || ctrl == CON_Right || ctrl == CON_Up || ctrl == CON_Down || ctrl == CON_Jump)
	{	
		// forward to script...
		if (house)
		{
			return ControlMovement2Script(ctrl, x, y, strength, repeat, release, house);
		}
		else if (vehicle)
		{
			if (ControlMovement2Script(ctrl, x, y, strength, repeat, release, vehicle)) return true;
		}
	
		return ObjectControlMovement(plr, ctrl, strength, release);
	}
	
	// hotkeys action bar hotkeys
	var hot = 0;
	if (ctrl == CON_InteractionHotkey0) hot = 10;
	if (ctrl == CON_InteractionHotkey1) hot = 1;
	if (ctrl == CON_InteractionHotkey2) hot = 2;
	if (ctrl == CON_InteractionHotkey3) hot = 3;
	if (ctrl == CON_InteractionHotkey4) hot = 4;
	if (ctrl == CON_InteractionHotkey5) hot = 5;
	if (ctrl == CON_InteractionHotkey6) hot = 6;
	if (ctrl == CON_InteractionHotkey7) hot = 7;
	if (ctrl == CON_InteractionHotkey8) hot = 8;
	if (ctrl == CON_InteractionHotkey9) hot = 9;
	
	if (hot > 0)
	{
		this.control.hotkeypressed = true;
		this->~ControlHotkey(hot-1);
		this->~StopInteractionCheck(); // for GUI_Controller_ActionBar
		return true;
	}
	
	// Unhandled control
	return _inherited(plr, ctrl, x, y, strength, repeat, release, ...);
}

// A wrapper to SetCommand to catch special behaviour for some actions.
public func ObjectCommand(string command, object target, int tx, int ty, object target2)
{
	// special control for throw and jump
	// but only with controls, not with general commands
	if (command == "Throw") return this->~ControlThrow(target,tx,ty);
	else if (command == "Jump") return this->~ControlJump();
	// else standard command
	else 
	{
		// Make sure to not recollect the item immediately on drops.
		if (command == "Drop")
		{
			// Disable collection for a moment.
			if (target) this->OnDropped(target);
		}
		return SetCommand(command,target,tx,ty,target2);
	}
	// this function might be obsolete: a normal SetCommand does make a callback to
	// script before it is executed: ControlCommand(szCommand, pTarget, iTx, iTy)
}

/*
	Called by the engine before a command is executed.
	Beware that this is NOT called when SetCommand was called by a script.
	At this point I am not sure whether we need this callback at all.
*/
public func ControlCommand(string command, object target, int tx, int ty)
{
	if (command == "Drop")
	{
		// Disable collection for a moment.
		if (target) this->OnDropped(target);
	}
	return _inherited(command, target, tx, ty, ...);
}

public func ShelveCommand(object condition_obj, string condition, object callback_obj, string callback, proplist data)
{
	this.control.shelved_command = { cond = condition, cond_obj = condition_obj, callback = callback, callback_obj = callback_obj, data = data };
	AddEffect("ShelvedCommand", this, 1, 5, this);
}

public func StopShelvedCommand()
{
	this.control.shelved_command = nil;
	if(GetEffect("ShelvedCommand", this))
		RemoveEffect("ShelvedCommand", this);
}

func FxShelvedCommandTimer(_, effect, time)
{
	if(!this.control.shelved_command) return -1;
	if(!this.control.shelved_command.callback_obj) return -1;
	if(!this.control.shelved_command.cond_obj) return -1;
	if(!this.control.shelved_command.cond_obj->Call(this.control.shelved_command.cond, this.control.shelved_command.data)) return 1;
	this.control.shelved_command.callback_obj->Call(this.control.shelved_command.callback, this.control.shelved_command.data);
	return -1;
}

func FxShelvedCommandStop(target, effect, reason, temp)
{
	if(temp) return;
	this.control.shelved_command = nil;
}

/* ++++++++++++++++++++++++ Use Controls ++++++++++++++++++++++++ */

public func CancelUse()
{
	if (!this.control.current_object)
	{
		// just forget any possibly stored actions
		StopShelvedCommand();
		return;
	}

	// use the saved x,y coordinates for canceling
	CancelUseControl(this.control.mlastx, this.control.mlasty);
}

// to be called during usage of an object to re-start usage as soon as possible
func PauseUse(object obj, string custom_condition, proplist data)
{
	// cancel use first, since it removes old shelved commands
	if(this.control.started_use)
	{
		CancelUse();
		this.control.started_use = false;
	}
	
	var callback_obj = this;
	
	if(custom_condition != nil)
	{
		callback_obj = obj;
	}
	else custom_condition = "CanReIssueCommand";
	
	data = data ?? {};
	data.obj = obj;
	data.ctrl = CON_Use;
	ShelveCommand(callback_obj, custom_condition, this, "ReIssueCommand", data);
}

func DetermineUsageType(object obj)
{
	if(!obj) return nil;
	// house
	if (obj == Contained())
		return C4D_Structure;
	// object
	if (obj->Contained() == this)
		return C4D_Object;
	// vehicle
	var proc = GetProcedure();
	if (obj == GetActionTarget())
		if (proc == "ATTACH" && proc == "PUSH")
			return C4D_Vehicle;
	// unknown
	return nil;
}

func GetUseCallString(string action)
{
	// Control... or Contained...
	var control = "Control";
	if (this.control.using_type == C4D_Structure) control = "Contained";
	// Action
	if (!action) action = "";
	
	return Format("~%sUse%s",control,action);
}

func CanReIssueCommand(proplist data)
{
	if(data.ctrl == CON_Use)
		return !data.obj->~RejectUse(this);
	
	if(data.ctrl == CON_UseDelayed)
		return !data.obj->~RejectUse(this);
}

func ReIssueCommand(proplist data)
{
	if(data.ctrl == CON_Use)
		return StartUseControl(data.ctrl, this.control.mlastx, this.control.mlasty, data.obj);
	
	if(data.ctrl == CON_UseDelayed)
		return StartUseDelayedControl(data.ctrl, data.obj);
}

func StartUseControl(int ctrl, int x, int y, object obj)
{
	this.control.started_use = false;
	
	if(obj->~RejectUse(this))
	{
		// remember for later:
		PauseUse(obj);
		// but still catch command
		return true;
	}
	
	this.control.current_object = obj;
	this.control.using_type = DetermineUsageType(obj);
	
	var hold_enabled = obj->Call("~HoldingEnabled");
	
	if (hold_enabled)
		SetPlayerControlEnabled(GetOwner(), CON_Aim, true);

	// first call UseStart. If unhandled, call Use (mousecontrol)
	var handled = obj->Call(GetUseCallString("Start"),this,x,y);
	if (!handled)
	{
		handled = obj->Call(GetUseCallString(),this,x,y);
		this.control.noholdingcallbacks = handled;
	}
	
	if (!handled)
	{
		this.control.current_object = nil;
		this.control.using_type = nil;
		if (hold_enabled)
			SetPlayerControlEnabled(GetOwner(), CON_Aim, false);
		return false;
	}
	else
	{
		this.control.started_use = true;
		// add helper effect that prevents errors when objects are suddenly deleted by quickly cancelling their use beforehand
		AddEffect("ItemRemovalCheck", this.control.current_object, 1, 100, this, nil); // the slow timer is arbitrary and will just clean up the effect if necessary
	}
		
	return handled;
}

func StartUseDelayedControl(int ctrl, object obj)
{
	this.control.started_use = false;
	
	if(obj->~RejectUse(this))
	{
		// remember for later:
		ShelveCommand(this, "CanReIssueCommand", this, "ReIssueCommand", {obj = obj, ctrl = ctrl});
		// but still catch command
		return true;
	}
	
	this.control.current_object = obj;
	this.control.using_type = DetermineUsageType(obj);
				
	VirtualCursor()->StartAim(this);
			
	// call UseStart
	var handled = obj->Call(GetUseCallString("Start"),this,this.control.mlastx,this.control.mlasty);
	this.control.noholdingcallbacks = !handled;
	
	if(handled)
		this.control.started_use = true;
		
	return handled;
}

func CancelUseControl(int x, int y)
{
	// forget possibly stored commands
	StopShelvedCommand();
	
	// to horse first (if there is one)
	var horse = GetActionTarget();
	if(horse && GetProcedure() == "ATTACH" && this.control.current_object != horse)
		StopUseControl(x, y, horse, true);

	return StopUseControl(x, y, this.control.current_object, true);
}

func StopUseControl(int x, int y, object obj, bool cancel)
{
	var stop = "Stop";
	if (cancel) stop = "Cancel";
	
	// ControlUseStop, ContainedUseCancel, etc...
	var handled = obj->Call(GetUseCallString(stop),this,x,y);
	if (obj == this.control.current_object)
	{
		// if ControlUseStop returned -1, the current object is kept as "used object"
		// but no more callbacks except for ControlUseCancel are made. The usage of this
		// object is finally cancelled on ControlUseCancel.
		if(cancel || handled != -1)
		{
			// look for correct removal helper effect and remove it
			var effect_index = 0;
			var removal_helper = nil;
			do
			{
				removal_helper = GetEffect("ItemRemovalCheck", this.control.current_object, effect_index++);
				if (!removal_helper) break;
				if (removal_helper.CommandTarget != this) continue;
				break;
			} while (true);
			
			this.control.current_object = nil;
			this.control.using_type = nil;
			
			if (removal_helper)
			{
				RemoveEffect(nil, nil, removal_helper, true);
			}		
		}
		this.control.noholdingcallbacks = false;
		
		SetPlayerControlEnabled(GetOwner(), CON_Aim, false);

		if (virtual_cursor)
			virtual_cursor->StopAim();
	}
		
	return handled;
}

func HoldingUseControl(int ctrl, int x, int y, object obj)
{
	var mex = x;
	var mey = y;
	if (ctrl == CON_UseDelayed)
	{
		mex = this.control.mlastx;
		mey = this.control.mlasty;
	}
	
	//Message("%d,%d",this,mex,mey);

	// automatic adjustment of the direction
	// --------------------
	// if this is desired for ALL objects is the question, we will find out.
	// For now, this is done for items and vehicles, not for buildings and
	// mounts (naturally). If turning vehicles just like that without issuing
	// a turning animation is the question. But after all, the clonk can turn
	// by setting the dir too.
	
	
	//   not riding and                not in building  not while scaling
	if (GetProcedure() != "ATTACH" && !Contained() &&   GetProcedure() != "SCALE")
	{
		// pushing vehicle: object to turn is the vehicle
		var dir_obj = GetActionTarget();
		if (GetProcedure() != "PUSH") dir_obj = nil;

		// otherwise, turn the clonk
		if (!dir_obj) dir_obj = this;
	
		if ((dir_obj->GetComDir() == COMD_Stop && dir_obj->GetXDir() == 0) || dir_obj->GetProcedure() == "FLIGHT")
		{
			if (dir_obj->GetDir() == DIR_Left)
			{
				if (mex > 0)
					dir_obj->SetDir(DIR_Right);
			}
			else
			{
				if (mex < 0)
					dir_obj->SetDir(DIR_Left);
			}
		}
	}
	
	var handled = obj->Call(GetUseCallString("Holding"),this,mex,mey);
			
	return handled;
}

func StopUseDelayedControl(object obj)
{
	// ControlUseStop, etc...
	
	var handled = obj->Call(GetUseCallString("Stop"),this,this.control.mlastx,this.control.mlasty);
	if (!handled)
		handled = obj->Call(GetUseCallString(),this,this.control.mlastx,this.control.mlasty);
	
	if (obj == this.control.current_object)
	{
		VirtualCursor()->StopAim();
		// see StopUseControl
		if(handled != -1)
		{
			this.control.current_object = nil;
			this.control.using_type = nil;
		}
		this.control.noholdingcallbacks = false;
	}
		
	return handled;
}

// very infrequent timer to prevent dangling effects, this is not necessary for correct functioning
func FxItemRemovalCheckTimer(object target, proplist effect, int time)
{
	if (!effect.CommandTarget) return -1;
	if (effect.CommandTarget.control.current_object != target) return -1;
	return 1;
}

// this will be called when an inventory object (that is in use) is suddenly removed
func FxItemRemovalCheckStop(object target, proplist effect, int reason, bool temporary)
{
	if (temporary) return;
	// only trigger when the object has been removed
	if (reason != FX_Call_RemoveClear) return;
	// safety
	if (!effect.CommandTarget) return;
	if (effect.CommandTarget.control.current_object != target) return;
	// quickly cancel use in a clean way while the object is still available
	effect.CommandTarget->CancelUse();
	return;
}


// Control use redirected to script
func ControlUse2Script(int ctrl, int x, int y, int strength, bool repeat, bool release, object obj)
{	
	// standard use
	if (ctrl == CON_Use)
	{
		if (!release && !repeat)
		{
			return StartUseControl(ctrl,x, y, obj);
		}
		else if (release && obj == this.control.current_object)
		{
			return StopUseControl(x, y, obj);
		}
	}
	// gamepad use
	else if (ctrl == CON_UseDelayed)
	{
		if (!release && !repeat)
		{
			return StartUseDelayedControl(ctrl, obj);
		}
		else if (release && obj == this.control.current_object)
		{
			return StopUseDelayedControl(obj);
		}
	}
	
	// more use (holding)
	if (ctrl == CON_Use || ctrl == CON_UseDelayed)
	{
		if (release)
		{
			// leftover use release
			CancelUse();
			return true;
		}
		else if (repeat && !this.control.noholdingcallbacks)
		{
			return HoldingUseControl(ctrl, x, y, obj);
		}
	}
		
	return false;
}

// Control use redirected to script
func ControlMovement2Script(int ctrl, int x, int y, int strength, bool repeat, bool release, object obj)
{
	// overloads of movement commandos
	if (ctrl == CON_Left || ctrl == CON_Right || ctrl == CON_Down || ctrl == CON_Up || ctrl == CON_Jump)
	{
		var control = "Control";
		if (Contained() == obj) control = "Contained";
	
		if (release)
		{
			// if any movement key has been released, ControlStop is called
			if (obj->Call(Format("~%sStop",control),this,ctrl))  return true;
		}
		else
		{
			// what about gamepad-deadzone?
			if (strength != nil && strength < CON_Gamepad_Deadzone)
				return true;
			
			// Control*
			if (ctrl == CON_Left)  if (obj->Call(Format("~%sLeft",control),this))  return true;
			if (ctrl == CON_Right) if (obj->Call(Format("~%sRight",control),this)) return true;
			if (ctrl == CON_Up)    if (obj->Call(Format("~%sUp",control),this))    return true;
			if (ctrl == CON_Down)  if (obj->Call(Format("~%sDown",control),this))  return true;
			
			// for attached (e.g. horse: also Jump command
			if (GetProcedure() == "ATTACH")
				if (ctrl == CON_Jump)  if (obj->Call("ControlJump",this)) return true;
		}
	}

}

// returns true if the clonk is able to enter a building (procedurewise)
public func CanEnter()
{
	var proc = GetProcedure();
	if (proc != "WALK" && proc != "SWIM" && proc != "SCALE" &&
		proc != "HANGLE" && proc != "FLOAT" && proc != "FLIGHT" &&
		proc != "PUSH") return false;
	return true;
}

public func IsMounted() { return GetProcedure() == "ATTACH"; }

/* +++++++++++++++++++++++ Menu control +++++++++++++++++++++++ */

local menu;

func HasMenuControl()
{
	return true;
}

// helper function that can be attached to a proplist to set callbacks on-the-fly
func GetTrue() { return true; }

/*
Sets the menu this Clonk currently has focus of. Old menus that have been opened via SetMenu will be closed, making sure that only one menu is open at a time.
Additionally, the Clonk's control is disabled while a menu is open.
The menu parameter can either be an object that closes its menu via a Close() callback or it can be a menu ID as returned by GuiOpen. When /menu/ is such an ID,
the menu will be closed via GuiClose when a new menu is opened. If you need to do cleaning up, you will have to use the OnClose callback of the menu.
When you call SetMenu with a menu ID, you should also call clonk->MenuClosed(), once your menu is closed.
*/
func SetMenu(new_menu, bool unclosable)
{
	unclosable = unclosable ?? false;
	
	// no news?
	if (new_menu) // if new_menu==nil, it is important that we still do the cleaning-up below even if we didn't have a menu before (see MenuClosed())
		if (menu == new_menu) return;
	
	// close old one!
	if (menu != nil)
	{
		if (GetType(menu) == C4V_C4Object)
			menu->Close();
		else if (GetType(menu) == C4V_PropList)
			GuiClose(menu.ID);
		else
			FatalError("Library_ClonkControl::SetMenu() was called with invalid parameter.");
	}
	else
	{
		// we have a new menu but didn't have another one before? Enable menu controls!
		if (new_menu)
		{
			CancelUse();
			// stop clonk
			SetComDir(COMD_Stop);
		
			if (PlayerHasVirtualCursor(GetOwner()))
				VirtualCursor()->StartAim(this,false, new_menu);
			else
			{
				if (GetType(new_menu) == C4V_C4Object && new_menu->~CursorUpdatesEnabled()) 
					SetPlayerControlEnabled(GetOwner(), CON_GUICursor, true);
		
				SetPlayerControlEnabled(GetOwner(), CON_GUIClick1, true);
				SetPlayerControlEnabled(GetOwner(), CON_GUIClick2, true);
			}
		}
	}
	
	if (new_menu)
	{
		if (GetType(new_menu) == C4V_C4Object)
		{
			menu = new_menu;
		}
		else if (GetType(new_menu) == C4V_Int)
		{
			// add a proplist, so that it is always safe to call functions on clonk->GetMenu()
			menu =
			{
				ID = new_menu
			};
		}
		else
			FatalError("Library_ClonkControl::SetMenu called with invalid parameter!");
		
		// make sure the menu is unclosable even if it is just a GUI ID
		if (unclosable)
		{
			menu.Unclosable = Library_ClonkControl.GetTrue;
		}
	}
	else
	{
		// always disable cursors, even if no old menu existed, because it can happen that a menu removes itself and thus the Clonk never knows whether the cursors are active or not
		RemoveVirtualCursor(); // for gamepads
		SetPlayerControlEnabled(GetOwner(), CON_GUICursor, false);
		SetPlayerControlEnabled(GetOwner(), CON_GUIClick1, false);
		SetPlayerControlEnabled(GetOwner(), CON_GUIClick2, false);

		menu = nil;
	}
	return menu;
}

func MenuClosed()
{
	// make sure not to clean up the menu again
	menu = nil;
	// and remove cursors etc.
	SetMenu(nil);
}

/*
Returns the current menu or nil. If a menu is returned, it is always a proplist (but not necessarily an object).
Stuff like if (clonk->GetMenu()) clonk->GetMenu()->~IsClosable(); is always safe.
If you want to remove the menu, the suggested method is clonk->TryCancelMenu() to handle unclosable menus correctly.
*/
func GetMenu()
{
	return menu;
}

// Returns true when an existing menu was closed
func CancelMenu()
{
	if (menu)
	{
		SetMenu(nil);
		return true;
	}
	
	return false;
}

// Tries to cancel a non-unclosable menu. Returns true when there is no menu left after this call (even if there never was one).
func TryCancelMenu()
{
	if (!menu) return true;
	if (menu->~Unclosable()) return false;
	CancelMenu();
	return true;
}

/* +++++++++++++++  Throwing, jumping +++++++++++++++ */

// Throwing
func DoThrow(object obj, int angle)
{
	// parameters...
	var iX, iY, iR, iXDir, iYDir, iRDir;
	iX = 4; if (!GetDir()) iX = -iX;
	iY = Cos(angle,-4);
	iR = Random(360);
	iRDir = RandomX(-10,10);

	iXDir = Sin(angle,this.ThrowSpeed);
	iYDir = Cos(angle,-this.ThrowSpeed);
	// throw boost (throws stronger upwards than downwards)
	if (iYDir < 0) iYDir = iYDir * 13/10;
	if (iYDir > 0) iYDir = iYDir * 8/10;
	
	// add own velocity
	iXDir += GetXDir(100)/2;
	iYDir += GetYDir(100)/2;

	// throw
	obj->Exit(iX, iY, iR, 0, 0, iRDir);
	obj->SetXDir(iXDir,100);
	obj->SetYDir(iYDir,100);
	
	return true;
}

// custom throw
// implemented in Clonk.ocd/Animations.ocd
public func ControlThrow() { return _inherited(...); }

public func ControlJump()
{
	var ydir = 0;
	
	if (GetProcedure() == "WALK")
	{
		ydir = this.JumpSpeed;
	}
	
	if (InLiquid())
	{
		if (!GBackSemiSolid(0,-5))
			ydir = BoundBy(this.JumpSpeed * 3 / 5, 240, 380);
	}

	if (GetProcedure() == "SCALE")
	{
		ydir = this.JumpSpeed/2;
	}
	
	if (ydir && !Stuck())
	{
		SetPosition(GetX(),GetY()-1);

		//Wall kick
		if(GetProcedure() == "SCALE")
		{
			AddEffect("WallKick",this,1);
			SetAction("Jump");

			var xdir;
			if(GetDir() == DIR_Right)
			{
				xdir = -1;
				SetDir(DIR_Left);
			}
			else if(GetDir() == DIR_Left)
			{
				xdir = 1;
				SetDir(DIR_Right);
			}

			SetYDir(-ydir * GetCon(), 100 * 100);
			SetXDir(xdir * 17);
			return true;
		}
		//Normal jump
		else
		{
			SetAction("Jump");
			SetYDir(-ydir * GetCon(), 100 * 100);
			return true;
		}
	}
	return false;
}

func FxIsWallKickStart(object target, int num, bool temp)
{
	return 1;
}

/*
	returns an array containing proplists with informations about the interactable actions.
	The proplist properties are:
		interaction_object
		priority: used for sorting the objects in the action bar. Note that the returned objects are not yet sorted
		interaction_index: when an object has multiple defined interactions, this is the index
		extra_data: custom extra_data for an interaction specified by the object
		actiontype: the kind of interaction. One of the ACTIONTYPE_* constants
*/
func GetInteractableObjects()
{
	var possible_interactions = [];
	// find vehicles & structures & script interactables
	// Get custom interactions from the clonk
	// extra interactions are an array of proplists. proplists have to contain at least a function pointer "f", a description "desc" and an "icon" definition/object. Optional "front"-boolean for sorting in before/after other interactions.
	var extra_interactions = this->~GetExtraInteractions() ?? []; // if not present, just use []. Less error prone than having multiple if(!foo).
		
	// all except structures only if outside
	var can_only_use_container = !!Contained();

	// add extra-interactions
	if (!can_only_use_container)
	for(var interaction in extra_interactions)
	{
		PushBack(possible_interactions,
			{
				interaction_object = interaction.Object,
				priority = interaction.Priority,
				interaction_index = nil,
				extra_data = interaction,
				actiontype = ACTIONTYPE_EXTRA
			});
	}
	
	// add interactables (script interface)
	var interactables = FindObjects(
		Find_AtPoint(0, 0),
		Find_Or(Find_OCF(OCF_Grab), Find_Func("IsInteractable", this), Find_OCF(OCF_Entrance)),
		Find_NoContainer(), Find_Layer(GetObjectLayer()));
	for(var interactable in interactables)
	{
		var icnt = interactable->~GetInteractionCount() ?? 1;
		
		if (!can_only_use_container)
		{
			// first the script
			// one object could have a scripted interaction AND be a vehicle
			if (interactable->~IsInteractable(this))
				for(var j = 0; j < icnt; j++)
				{
					PushBack(possible_interactions,
						{
							interaction_object = interactable,
							priority = 9,
							interaction_index = j,
							extra_data = nil,
							actiontype = ACTIONTYPE_SCRIPT
						});
				}
			// check whether further interactions are possible
	
			// can be grabbed? (vehicles/chests..)
			if (interactable->GetOCF() & OCF_Grab)
			{
				var priority = 19;
				// high priority if already grabbed
				if (GetActionTarget() == interactable) priority = 0;
				
				PushBack(possible_interactions,
					{
						interaction_object = interactable,
						priority = priority,
						interaction_index = nil,
						extra_data = nil,
						actiontype = ACTIONTYPE_VEHICLE
					});
			}
		}
		
		// can be entered?
		if (interactable->GetOCF() & OCF_Entrance && (!can_only_use_container || interactable == Contained()))
		{
			var priority = 29;
			if (Contained() == interactable) priority = 0;
			PushBack(possible_interactions,
				{
					interaction_object = interactable,
					priority = priority,
					interaction_index = nil,
					extra_data = nil,
					actiontype = ACTIONTYPE_STRUCTURE
				});
		}
	}
	
	return possible_interactions;
}

// executes interaction with an object. /action_info/ is a proplist as returned by GetInteractableObjects
func ExecuteInteraction(proplist action_info)
{
	if (!action_info.interaction_object)
		return;
		
	// object is a pushable vehicle
	if(action_info.actiontype == ACTIONTYPE_VEHICLE)
	{
		var proc = GetProcedure();
		// object is inside building -> activate
		if(Contained() && action_info.interaction_object->Contained() == Contained())
		{
			SetCommand("Activate", action_info.interaction_object);
			return true;
		}
		// crew is currently pushing vehicle
		else if(proc == "PUSH")
		{
			// which is mine -> let go
			if(GetActionTarget() == action_info.interaction_object)
				ObjectCommand("UnGrab");
			else
				ObjectCommand("Grab", action_info.interaction_object);
				
			return true;
		}
		// grab
		else if(proc == "WALK")
		{
			ObjectCommand("Grab", action_info.interaction_object);
			return true;
		}
	}
	// object is a building
	else if (action_info.actiontype == ACTIONTYPE_STRUCTURE)
	{
		// inside? -> exit
		if(Contained() == action_info.interaction_object)
		{
			ObjectCommand("Exit");
			return true;
		}
		// outside? -> enter
		else if(this->CanEnter())
		{
			ObjectCommand("Enter", action_info.interaction_object);
			return true;
		}
	}
	else if (action_info.actiontype == ACTIONTYPE_SCRIPT)
	{
		if(action_info.interaction_object->~IsInteractable(this))
		{
			action_info.interaction_object->Interact(this, action_info.interaction_number);
			return true;
		}
	}
	else if (action_info.actiontype == ACTIONTYPE_EXTRA)
	{
		if(action_info.extra_data)
			action_info.extra_data.Object->Call(action_info.extra_data.Fn, this);
	}
}