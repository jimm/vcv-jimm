#include "Arpeggiator.hpp"
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

	Arpeggiator() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void step() override;
  json_t * toJson() override;
  void fromJson(json_t *rootJ) override;

	void onReset() override {
		for (int i = 0; i < 8; ++i)
			gateState[i] = true;
	}

	void onRandomize() override {
		for (int i = 0; i < 8; ++i)
			gateState[i] = (randomf() > 0.5);
	}

private:

  float note(int row);
  void advanceIndex();
};

void Arpeggiator::step() {
	const float lightLambda = 0.075;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;

	bool nextStep = false;

	if (running) {
		if (inputs[EXT_CLOCK_INPUT].active) {
			// External clock
			if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value)) {
				phase = 0.0;
				nextStep = true;
			}
		}
		else {
			// Internal clock
			float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
			phase += clockTime / engineGetSampleRate();
			if (phase >= 1.0) {
				phase -= 1.0;
				nextStep = true;
			}
		}
	}

	// Reset
	if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
		phase = 0.0;
		index = 8;
		nextStep = true;
		resetLight = 1.0;
	}

	if (nextStep) {
		// Advance current position
    directionMode = (DirectionMode)(2 - (int)params[DIRECTION_MODE_PARAM].value);
    advanceIndex();
		stepLights[index] = 1.0;
		gatePulse.trigger(1e-3);
	}

	resetLight -= resetLight / lightLambda / engineGetSampleRate();

	bool pulse = gatePulse.process(1.0 / engineGetSampleRate());

	// Gate buttons
	for (int i = 0; i < 8; ++i) {
		if (gateTriggers[i].process(params[GATE_PARAM + i].value)) {
			gateState[i] = !gateState[i];
		}
		bool gateOn = (running && i == index && gateState[i]);
		if (gateMode == TRIGGER)
			gateOn = gateOn && pulse;
		else if (gateMode == RETRIGGER)
			gateOn = gateOn && !pulse;

		outputs[GATE_OUTPUT + i].value = gateOn ? 10.0 : 0.0;
		stepLights[i] -= stepLights[i] / lightLambda / engineGetSampleRate();
		lights[GATE_LIGHTS + i].value = gateState[i] ? 1.0 - stepLights[i] : stepLights[i];
	}

	// Rows
	float row1 = params[ROW1_PARAM + index].value;
	float row2 = params[ROW2_PARAM + index].value;
	float row3 = params[ROW3_PARAM + index].value;
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
  for (int i = 0; i < 3; ++i)
    outputs[ROW1_OUTPUT + i].value = note(i);
	outputs[GATES_OUTPUT].value = gatesOn ? 10.0 : 0.0;
	lights[RESET_LIGHT].value = resetLight;
	lights[GATES_LIGHT].value = gatesOn ? 1.0 : 0.0;
	lights[ROW_LIGHTS    ].value = row1 / 10.0;
	lights[ROW_LIGHTS + 1].value = row2 / 10.0;
	lights[ROW_LIGHTS + 2].value = row3 / 10.0;
}

float Arpeggiator::note(int row) {
  float oct = params[ROW1_OCTAVE_PARAM + (row * 8) + index].value;
  float semi = params[ROW1_PARAM + (row * 8) + index].value;
  float pitch_offset = inputs[ROW1_PITCH_INPUT + row].value;

  int note = (oct + 2) * 12 + semi;
  return (note - 60)  / 12.0 + pitch_offset;
}

void Arpeggiator::advanceIndex() {
  int numSteps = clampi(roundf(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1, 8);

  switch (directionMode) {
  case UP:
    index += 1;
    if (index >= numSteps) {
      index = 0;
    }
    break;
  case UP_DOWN:
    switch (runningDirection) {
    case UP_DIR:
      index += 1;
      if (index >= numSteps) {
        index -= 2;
        if (index < 0)
          index = 0;
        runningDirection = DOWN_DIR;
      }
      break;
    case DOWN_DIR:
      index -= 1;
      if (index < 0) {
        index += 2;
        if (index >= numSteps) {
          index = numSteps;
        }
        runningDirection = UP_DIR;
      }
      break;
    }
    break;
  case RANDOM:
    do {
      index = randomf() * (numSteps + 1);
    } while (index >= numSteps);
    break;
  }
}

json_t * Arpeggiator::toJson() {
  json_t *rootJ = json_object();

  // running
  json_object_set_new(rootJ, "running", json_boolean(running));

  // octaves
  json_t *octsJ = json_array();
  for (int row = 0; row < 3; ++row) {
    json_t *row_octs = json_array();
    for (int i = 0; i < 8; ++i) {
      json_t *octJ = json_integer((int)params[ROW1_OCTAVE_PARAM + (row * 8) + i].value);
      json_array_append_new(row_octs, octJ);
    }
    json_array_append_new(octsJ, row_octs);
  }
  json_object_set_new(rootJ, "octaves", octsJ);

  // gates
  json_t *gatesJ = json_array();
  for (int i = 0; i < 8; ++i) {
    json_t *gateJ = json_integer((int) gateState[i]);
    json_array_append_new(gatesJ, gateJ);
  }
  json_object_set_new(rootJ, "gates", gatesJ);

  // gateMode
  json_t *gateModeJ = json_integer((int)gateMode);
  json_object_set_new(rootJ, "gateMode", gateModeJ);

  // directionMode
  json_t *directionModeJ = json_integer((int)directionMode);
  json_object_set_new(rootJ, "directionMode", directionModeJ);

  return rootJ;
}

void Arpeggiator::fromJson(json_t *rootJ) {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// octaves
		json_t *octsJ = json_object_get(rootJ, "octaves");
		if (octsJ) {
      for (int row = 0; row < 3; ++row) {
        json_t *row_octs = json_array_get(octsJ, row);
        for (int i = 0; i < 8; ++i) {
          json_t *octJ = json_array_get(row_octs, i);
          if (octJ)
            params[ROW1_OCTAVE_PARAM + (row * 8) + i].value = (float)json_integer_value(octJ);
        }
      }
    }

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 8; ++i) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);

		// directionMode
		json_t *directionModeJ = json_object_get(rootJ, "directionMode");
		if (directionModeJ)
			directionMode = (DirectionMode)json_integer_value(directionModeJ);
	}

// ================ widget ================

ArpeggiatorWidget::ArpeggiatorWidget() {
	Arpeggiator *module = new Arpeggiator();
	setModule(module);
	box.size = Vec(15*22, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/Arpeggiator.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundSmallBlackKnob>(Vec(18, 56), module, Arpeggiator::CLOCK_PARAM, -2.0, 6.0, 2.0));
	addParam(createParam<LEDButton>(Vec(60, 61-1), module, Arpeggiator::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<MediumLight<GreenLight>>(Vec(64.4, 64.4), module, Arpeggiator::RUNNING_LIGHT));
	addParam(createParam<LEDButton>(Vec(99, 61-1), module, Arpeggiator::RESET_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<MediumLight<GreenLight>>(Vec(103.4, 64.4), module, Arpeggiator::RESET_LIGHT));
	addParam(createParam<RoundSmallBlackSnapKnob>(Vec(132, 56), module, Arpeggiator::STEPS_PARAM, 1.0, 8.0, 8.0));
	addChild(createLight<MediumLight<GreenLight>>(Vec(179.4, 64.4), module, Arpeggiator::GATES_LIGHT));
	addChild(createLight<MediumLight<GreenLight>>(Vec(218.4, 64.4), module, Arpeggiator::ROW_LIGHTS));
	addChild(createLight<MediumLight<GreenLight>>(Vec(256.4, 64.4), module, Arpeggiator::ROW_LIGHTS + 1));
	addChild(createLight<MediumLight<GreenLight>>(Vec(295.4, 64.4), module, Arpeggiator::ROW_LIGHTS + 2));

	static const float portX[8] = {20, 58, 96, 135, 173, 212, 250, 289};
	static const float squishedPortX[8] = {60, 92, 124, 157, 191, 224, 256, 289};

	addInput(createInput<PJ301MPort>(Vec(portX[0]-1, 98), module, Arpeggiator::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX[1]-1, 98), module, Arpeggiator::EXT_CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX[2]-1, 98), module, Arpeggiator::RESET_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX[3]-1, 98), module, Arpeggiator::STEPS_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX[4]-1, 98), module, Arpeggiator::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX[5]-1, 98), module, Arpeggiator::ROW1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX[6]-1, 98), module, Arpeggiator::ROW2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX[7]-1, 98), module, Arpeggiator::ROW3_OUTPUT));

  addInput(createInput<PJ301MPort>(Vec(portX[0]-1, 165), module, Arpeggiator::ROW1_PITCH_INPUT));
  addInput(createInput<PJ301MPort>(Vec(portX[0]-1, 206), module, Arpeggiator::ROW2_PITCH_INPUT));
  addInput(createInput<PJ301MPort>(Vec(portX[0]-1, 248), module, Arpeggiator::ROW3_PITCH_INPUT));

	for (int i = 0; i < 8; ++i) {
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2, 157), module, Arpeggiator::ROW1_PARAM + i, 0.0, 11.0, 0.0));
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2, 198), module, Arpeggiator::ROW2_PARAM + i, 0.0, 11.0, 0.0));
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2, 240), module, Arpeggiator::ROW3_PARAM + i, 0.0, 11.0, 0.0));
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2 + 8, 157+16), module, Arpeggiator::ROW1_OCTAVE_PARAM + i, 0.0, 7.0, 4.0));
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2 + 8, 198+16), module, Arpeggiator::ROW1_OCTAVE_PARAM + i, 0.0, 7.0, 4.0));
		addParam(createParam<RoundTinyBlackSnapKnob>(Vec(squishedPortX[i]-2 + 8, 240+16), module, Arpeggiator::ROW1_OCTAVE_PARAM + i, 0.0, 7.0, 4.0));
		addParam(createParam<LEDButton>(Vec(squishedPortX[i]+2, 278-1), module, Arpeggiator::GATE_PARAM + i, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenLight>>(Vec(squishedPortX[i]+6.4, 281.4), module, Arpeggiator::GATE_LIGHTS + i));
		addOutput(createOutput<PJ301MPort>(Vec(squishedPortX[i]-1, 307), module, Arpeggiator::GATE_OUTPUT + i));
	}

  addParam(createParam<NKK>(Vec(26, 274), module, Arpeggiator::DIRECTION_MODE_PARAM, 0.0, 2.0, 0.0));
}

struct ArpeggiatorGateModeItem : MenuItem {
	Arpeggiator *arpeg;
	Arpeggiator::GateMode gateMode;
  ArpeggiatorGateModeItem(const char *s, Arpeggiator *a, Arpeggiator::GateMode gm)
    : arpeg(a), gateMode(gm)
    { text = s; }
	void onAction(EventAction &e) override {
		arpeg->gateMode = gateMode;
	}
	void step() override {
		rightText = (arpeg->gateMode == gateMode) ? "✔" : "";
	}
};

Menu *ArpeggiatorWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	Arpeggiator *arpeg = dynamic_cast<Arpeggiator*>(module);
	assert(arpeg);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

  menu->addChild(new ArpeggiatorGateModeItem("Trigger", arpeg, Arpeggiator::TRIGGER));
  menu->addChild(new ArpeggiatorGateModeItem("Retrigger", arpeg, Arpeggiator::RETRIGGER));
  menu->addChild(new ArpeggiatorGateModeItem("Continuous", arpeg, Arpeggiator::CONTINUOUS));

	return menu;

}