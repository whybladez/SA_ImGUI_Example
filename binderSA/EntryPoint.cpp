#include "Main.h"


class cEntry {
public:
	cEntry() {
		Plugin->initPlugin();
	}
	~cEntry()
	{
		// remove hooks etc.
	}
} cEntry;