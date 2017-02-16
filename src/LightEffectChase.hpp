#pragma once

#include <cmath>
#include <iostream> //For debugging

#include "ILightEffect.hpp"
#include "Color.hpp"

class LightEffectChase : public ILightEffect
{
public:
	LightEffectChase(Color c1, Color c2, int speed_ms);

	virtual void addNode(const std::shared_ptr<LightNode>&) override;

private:
	virtual void update() override;
	void slotStateChange(LightNode*, LightNode::State_e);
	

	//float brightness, speed, hue;
	Color c1, c2;
	int speed_ms;
	//single color for all attached lights
	//Color color;
};
