#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct ArpeggiatorWidget : ModuleWidget {
	ArpeggiatorWidget();
	Menu *createContextMenu() override;
};
