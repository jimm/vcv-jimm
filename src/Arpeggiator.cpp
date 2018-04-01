#include "Arpeggiator.hpp"

Arpeggiator::Arpeggiator() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
  onReset();
}

void Arpeggiator::onReset() {
		for (int i = 0; i < 8; ++i)
			gateState[i] = true;
}

void Arpeggiator::onRandomize() {
  for (int i = 0; i < 8; ++i)
    gateState[i] = (randomUniform() > 0.5);
}

void Arpeggiator::step() {
	if (runningTrigger.process(params[RUN_PARAM].value))
		running = !running;

	bool nextStep = false;

  bool gateIn = false;
	if (running) {
		if (inputs[EXT_CLOCK_INPUT].active) {
			// External clock
			if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value)) {
				phase = 0.0;
				nextStep = true;
			}
      gateIn = clockTrigger.isHigh();
		}
		else {
			// Internal clock
			float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
			phase += clockTime / engineGetSampleRate();
			if (phase >= 1.0) {
				phase -= 1.0;
				nextStep = true;
			}
      gateIn = (phase < 0.5f);
		}
	}

	// Reset
	if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
		phase = 0.0;
		index = 8;
		nextStep = true;
	}

	if (nextStep) {
		// Advance current position
    directionMode = (DirectionMode)(2 - (int)params[DIRECTION_MODE_PARAM].value);
    advanceIndex();
		stepLights[index] = 1.0;
		gatePulse.trigger(1e-3);
	}

	bool pulse = gatePulse.process(1.0 / engineGetSampleRate());

	// Gate buttons
	for (int i = 0; i < 8; ++i) {
			if (gateTriggers[i].process(params[GATE_PARAM + i].value)) {
				gateState[i] = !gateState[i];
			}
			outputs[GATE_OUTPUT + i].value = (running && gateIn && i == index && gateState[i]) ? 10.0f : 0.0f;
			lights[GATE_LIGHTS + i].setBrightnessSmooth((gateIn && i == index) ? (gateState[i] ? 1.f : 0.33) : (gateState[i] ? 0.66 : 0.0));
	}

	// Rows
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
  outputs[ROW1_OUTPUT].value = params[ROW1_PARAM + index].value;
  outputs[ROW2_OUTPUT].value = params[ROW2_PARAM + index].value;
  outputs[ROW3_OUTPUT].value = params[ROW3_PARAM + index].value;
	outputs[GATES_OUTPUT].value = (gateIn && gateState[index]) ? 10.0f : 0.0f;
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;
	lights[RESET_LIGHT].setBrightnessSmooth(resetTrigger.isHigh());
	lights[GATES_LIGHT].setBrightnessSmooth(gateIn);
  lights[ROW_LIGHTS    ].value = outputs[ROW1_OUTPUT].value / 10.0f;
  lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.0f;
  lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.0f;
}

float Arpeggiator::note(int row) {
  float oct = params[ROW1_OCTAVE_PARAM + (row * 8) + index].value;
  float semi = params[ROW1_PARAM + (row * 8) + index].value;
  float pitch_offset = inputs[ROW1_PITCH_INPUT + row].value;

  int note = (oct + 2) * 12 + semi;
  return (note - 60)  / 12.0 + pitch_offset;
}

void Arpeggiator::advanceIndex() {
  int numSteps = clamp(int(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1, 8);

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
      index = randomu32() % numSteps;
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

ArpeggiatorWidget::ArpeggiatorWidget(Arpeggiator *module) : ModuleWidget(module) {
	box.size = Vec(15*22, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/Arpeggiator.svg")));
		addChild(panel);
	}

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(ParamWidget::create<RoundBlackKnob>(Vec(19, 56), module, Arpeggiator::CLOCK_PARAM, -2.0, 6.0, 2.0));
	addParam(ParamWidget::create<LEDButton>(Vec(60, 61-1), module, Arpeggiator::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(64.4, 64.4), module, Arpeggiator::RUNNING_LIGHT));
	addParam(ParamWidget::create<LEDButton>(Vec(99, 61-1), module, Arpeggiator::RESET_PARAM, 0.0, 1.0, 0.0));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(103.4, 64.4), module, Arpeggiator::RESET_LIGHT));
	addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(134, 56), module, Arpeggiator::STEPS_PARAM, 1.0, 8.0, 8.0));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(179.4, 64.4), module, Arpeggiator::GATES_LIGHT));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(218.4, 64.4), module, Arpeggiator::ROW_LIGHTS));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(256.4, 64.4), module, Arpeggiator::ROW_LIGHTS + 1));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(295.4, 64.4), module, Arpeggiator::ROW_LIGHTS + 2));

	static const float portX[8] = {20, 58, 96, 135, 173, 212, 250, 289};
	static const float squishedPortX[8] = {60, 92, 124, 157, 191, 224, 256, 289};

	addInput(Port::create<PJ301MPort>(Vec(portX[0]-1, 98), Port::INPUT, module, Arpeggiator::CLOCK_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(portX[1]-1, 98), Port::INPUT, module, Arpeggiator::EXT_CLOCK_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(portX[2]-1, 98), Port::INPUT, module, Arpeggiator::RESET_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(portX[3]-1, 98), Port::INPUT, module, Arpeggiator::STEPS_INPUT));
	addOutput(Port::create<PJ301MPort>(Vec(portX[4]-1, 98), Port::OUTPUT, module, Arpeggiator::GATES_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(portX[5]-1, 98), Port::OUTPUT, module, Arpeggiator::ROW1_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(portX[6]-1, 98), Port::OUTPUT, module, Arpeggiator::ROW2_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(portX[7]-1, 98), Port::OUTPUT, module, Arpeggiator::ROW3_OUTPUT));

  addInput(Port::create<PJ301MPort>(Vec(portX[0]-1, 165), Port::INPUT, module, Arpeggiator::ROW1_PITCH_INPUT));
  addInput(Port::create<PJ301MPort>(Vec(portX[0]-1, 206), Port::INPUT, module, Arpeggiator::ROW2_PITCH_INPUT));
  addInput(Port::create<PJ301MPort>(Vec(portX[0]-1, 248), Port::INPUT, module, Arpeggiator::ROW3_PITCH_INPUT));

	for (int i = 0; i < 8; ++i) {
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] - 4, 157 -  4), module, Arpeggiator::ROW1_PARAM + i, 0.0f, 11.0f, 0.0f));
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] - 4, 198 -  4), module, Arpeggiator::ROW2_PARAM + i, 0.0f, 11.0f, 0.0f));
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] - 4, 240 -  4), module, Arpeggiator::ROW3_PARAM + i, 0.0f, 11.0f, 0.0f));
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] + 6, 157 + 14), module, Arpeggiator::ROW1_OCTAVE_PARAM + i, 0.0f, 7.0f, 4.0f));
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] + 6, 198 + 14), module, Arpeggiator::ROW2_OCTAVE_PARAM + i, 0.0f, 7.0f, 4.0f));
		addParam(ParamWidget::create<SnapTrimpot>(Vec(squishedPortX[i] + 6, 240 + 14), module, Arpeggiator::ROW3_OCTAVE_PARAM + i, 0.0f, 7.0f, 4.0f));
		addParam(ParamWidget::create<LEDButton>(Vec(squishedPortX[i]+2, 278-1), module, Arpeggiator::GATE_PARAM + i, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(squishedPortX[i]+6.4f, 281.4f), module, Arpeggiator::GATE_LIGHTS + i));
		addOutput(Port::create<PJ301MPort>(Vec(squishedPortX[i]-1, 307), Port::OUTPUT, module, Arpeggiator::GATE_OUTPUT + i));
	}

  addParam(ParamWidget::create<NKK>(Vec(26, 274), module, Arpeggiator::DIRECTION_MODE_PARAM, 0.0f, 2.0f, 0.0f));
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
