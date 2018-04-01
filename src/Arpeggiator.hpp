#pragma once

#include "Jimm.hpp"
#include "dsp/digital.hpp"

struct Arpeggiator : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
    MODE_PARAM,
		ROW1_PARAM,
		ROW2_PARAM = ROW1_PARAM + 8,
		ROW3_PARAM = ROW2_PARAM + 8,
    ROW1_OCTAVE_PARAM = ROW3_PARAM + 8,
    ROW2_OCTAVE_PARAM = ROW1_OCTAVE_PARAM + 8,
    ROW3_OCTAVE_PARAM = ROW2_OCTAVE_PARAM + 8,
		GATE_PARAM = ROW3_OCTAVE_PARAM + 8,
    DIRECTION_MODE_PARAM = GATE_PARAM + 8,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
    ROW1_PITCH_INPUT,
    ROW2_PITCH_INPUT,
    ROW3_PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		ROW1_OUTPUT,
		ROW2_OUTPUT,
		ROW3_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS = GATE_OUTPUT + 8
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		ROW_LIGHTS,
		GATE_LIGHTS = ROW_LIGHTS + 3,
		NUM_LIGHTS = GATE_LIGHTS + 8
	};

	bool running = true;
	SchmittTrigger clockTrigger; // for external clock
	// For buttons
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger gateTriggers[8];
	float phase = 0.0;
	int index = 0;
	bool gateState[8] = {};
	float resetLight = 0.0;
	float stepLights[8] = {};

	enum GateMode {
		TRIGGER,
		RETRIGGER,
		CONTINUOUS,
	};
	GateMode gateMode = TRIGGER;
	PulseGenerator gatePulse;

	enum DirectionMode {
		UP,
		UP_DOWN,
		RANDOM,
	};
  DirectionMode directionMode = UP;
  enum RunningDirection {
    UP_DIR,
    DOWN_DIR,
  };
  RunningDirection runningDirection = UP_DIR;

	Arpeggiator();

	void step() override;
  json_t * toJson() override;
  void fromJson(json_t *rootJ) override;
	void onReset() override;
	void onRandomize() override;

private:

  float note(int row);
  void advanceIndex();
};

struct ArpeggiatorWidget : ModuleWidget {
	ArpeggiatorWidget(Arpeggiator *module);
	Menu *createContextMenu() override;
};

struct SnapTrimpot : Trimpot {
	SnapTrimpot() : Trimpot() {
		snap = true;
    smooth = false;
	}
};
