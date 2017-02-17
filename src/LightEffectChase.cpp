#include "LightEffectChase.hpp"
#include "debug.h"

LightEffectChase::LightEffectChase(Color c1, Color c2, int speed_ms)
	:	c1{c1}
	,   c2{c2}
	,	speed_ms{speed_ms} {

}

void LightEffectChase::addNode(const std::shared_ptr<LightNode>& node) {
	nodes.push_back(node);

	//Add listener for state changes
	node->addListener(LightNode::STATE_CHANGE,
		std::bind(&LightEffectChase::slotStateChange, this, std::placeholders::_1,
		std::placeholders::_3));

	debug << "Setting new node '"	<< node->getName() << "' C1=" << c1.toString() << ", C2=" <<c2.toString() << std::endl;

}

void LightEffectChase::update() {
	static int count = 0;
	
	if(count < speed_ms){ count += 20; return;}
	count = 0;

	//Update all of the nodes
	for(auto& node : nodes) {
		LightStrip& strip = node->getLightStrip();//.setAll(color);
		Color temp = strip.getPixel(0);
		int i;
		for(i = 0; i < strip.getSize() - 1; i++) {
			strip.setPixel(i, strip.getPixel(i+1));
		}
		strip.setPixel(i, temp);
		
		//Release the light strip resource
		node->releaseLightStrip();
	}
}

void LightEffectChase::slotStateChange(LightNode* node,
	LightNode::State_e newState) {

	debug << "newState==" << LightNode::stateToString(newState) <<std::endl;

	if(newState == LightNode::CONNECTED) {
		//Update the pixels
		//node->getLightStrip().setAll(color);

		LightStrip& strip = node->getLightStrip();

		for (int i = 0; i < strip.getSize(); i++) {
			if ( (i%8) < 4 ) {
				strip.setPixel(i, c1);
			} else {
				strip.setPixel(i, c2);
			}
		}
		std::cout << std::endl;

		//Release the strip
		node->releaseLightStrip();
	}
}
