// Prevents the clonk from collecting items other than the weapons.#appendto Clonkprotected func RejectCollect(id objid, object obj){	if (objid != Bow && objid != Arrow && objid != Javelin)		return true;	return _inherited(objid, obj);}