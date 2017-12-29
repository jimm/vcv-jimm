#pragma once
#include "Jimm.hpp"

struct RoundTinyBlackKnob : RoundBlackKnob {
	RoundTinyBlackKnob() {
		box.size = Vec(16, 16);
	}
};

struct RoundTinyBlackSnapKnob : RoundTinyBlackKnob {
	RoundTinyBlackSnapKnob() {
		snap = true;
	}
};
