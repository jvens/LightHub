#include "SoundColor.hpp"

#include <algorithm>
#include <iostream>
#include <functional>
#include <sstream>

#include <cstdio> //For printf

using namespace std;

std::string SoundColorSettings::toString() {
	std::stringstream ss;

	ss << "bassFreq = " << bassFreq << '\n'
		<< "trebbleFreq = " << trebbleFreq << '\n'
		<< "bassBoost = " << bassBoost << '\n'
		<< "trebbleBoost = " << trebbleBoost << '\n'
		<< "fStart = " << fStart << '\n'
		<< "fEnd = " << fEnd << '\n'
		<< "dbScaler = " << dbScaler << '\n'
		<< "dbFactor = " << dbFactor << '\n'
		<< "avgFactor = " << avgFactor << '\n'
		<< "noiseFloor = " << noiseFloor << '\n'
		<< "avgFilterStrength = " << avgFilterStrength << '\n'
		<< "minSaturation = " << minSaturation << '\n'
		<< "filterStrength = " << filterStrength << '\n'
		<< "centerSpread = " << centerSpread << '\n';
	
	return ss.str();
}


SoundColor::SoundColor(std::shared_ptr<SpectrumAnalyzer> _spectrumAnalyzer,
	const SoundColorSettings& _settings)
	:	settings(_settings)
	,	spectrumAnalyzer{_spectrumAnalyzer}
	,	hasChanged{false} {

	std::cout << settings.toString() << std::endl;

	//Get the spectrum to calculate the color vector
	auto spectrum = spectrumAnalyzer->getLeftSpectrum();

	//Determine the corresponding bin to freqBass
	size_t bassBin = 0;
	for(; bassBin < spectrum->getBinCount(); ++bassBin) {
		auto& bin = spectrum->getByIndex(bassBin);

		if(bin.getFreqCenter() >= settings.bassFreq)
			break;
	}

	//bassBin minimum of 2
	if(bassBin < 2)
		bassBin = 2;
	
	//Determine the corresponding bin to freqTrebble
	size_t trebbleBin = 0;
	for(trebbleBin = bassBin; trebbleBin < spectrum->getBinCount(); ++trebbleBin) {
		auto& bin = spectrum->getByIndex(trebbleBin);

		if(bin.getFreqCenter() >= settings.trebbleFreq)
			break;
	}

	for(size_t i = 0; i < bassBin; i++) {
		double hue = i * 30. / (bassBin);

		Color c = Color::HSV(hue, 1., 1.);

		frequencyColors.push_back(c);

		std::cout << hue << ":\t" << c.toString() << '\n';
	}

	for(size_t i = bassBin; i < spectrum->getBinCount(); i++) {
		double hue = 30. + (i - bassBin) * 210. /
			(spectrum->getBinCount() - bassBin - 1);

		Color c = Color::HSV(hue, 1., 1.);

		frequencyColors.push_back(c);

		std::cout << hue << ":\t" << c.toString() << '\n';
	}

	std::cout << std::endl;

	//Initialize each color channel
	left.avg = 0.;
	center.avg = 0.;
	right.avg = 0.;

	//spectrumAnalyzer->addListener(std::bind(SoundColor::cbSpectrum, this));
	spectrumAnalyzer->addListener([this](SpectrumAnalyzer* sa,
		std::shared_ptr<Spectrum> left, std::shared_ptr<Spectrum> right) {
		cbSpectrum(sa, left, right);
	});
}

SoundColor::~SoundColor() {
	//Remove the spectrum listener
	//spectrumAnalyzer->removeListener(std::bind(&SoundColor::cbSpectrum, this));
}

bool SoundColor::changed() {
	return hasChanged;
}

void SoundColor::getColor(Color* _left, Color* _center, Color* _right) {
	*_left = left.c;
	*_center = center.c;
	*_right = right.c;

	hasChanged = false;
}

void SoundColor::cbSpectrum(SpectrumAnalyzer*,
	std::shared_ptr<Spectrum> leftSpectrum,
	std::shared_ptr<Spectrum> rightSpectrum) {

	//Render color for left, right channels
	renderColor(left, *leftSpectrum.get());
	//renderColor(right, *rightSpectrum.get());

	std::cout << left.c.toString() << std::endl;


	hasChanged = true;
}

void SoundColor::renderColor(ColorChannel& prevColor, Spectrum& spectrum) {

	double r = 0., g = 0., b = 0.;
	size_t binCount = spectrum.getBinCount();

	//double curAvg = spectrum.getAverageEnergyDB() + settings.noiseFloor;

	//if(curAvg < 0)
		//curAvg = 0;

	double absFloor = std::pow(10., -100);
	double absBoost = std::pow(10., settings.bassBoost/20.);

	cout << "[Info] Noise Floor: " << absFloor << '\n';
	cout << "[Info] Bass Boost: " << absBoost << '\n';

	double curAvg = spectrum.getAverageEnergy();

	cout << "[Info] Average energy: " << curAvg << endl;

	if(curAvg < absFloor)
		curAvg = absFloor;
	


	//Loosely track the average DB
	/*if(prevColor.avg > curAvg) {
		//Slope limiting
		prevColor.avg -= std::min(settings.slopeLimitAvg, prevColor.avg - curAvg);
	}
	else {
		//Jump up to current average
		//prevColor.avg = curAvg;

		//Slope limiting
		prevColor.avg += std::min(settings.slopeLimitAvg, curAvg - prevColor.avg);
	}*/
	prevColor.avg = prevColor.avg*settings.avgFilterStrength
		+ curAvg*(1. - settings.avgFilterStrength);
	


	//Scale to be applied to each bin
	//double scale = 1. / settings.dbScaler;
	double scale = 500;


	for(unsigned int i = 0; i < binCount; ++i) {
		FrequencyBin& bin = spectrum.getByIndex(i);
		double f = bin.getFreqCenter();

		if(f > settings.fEnd)
			break;

		if(f >= settings.fStart) {
			Color c = frequencyColors[i];
			//Color c = Color::HSV(240.f * i / (binCount - 1), 1.f, 1.f);
			double db = bin.getEnergyDB();
			double energy = bin.getEnergy();

			//Bass boost
			//if(f <= settings.bassFreq)
				//db += settings.bassBoost;

			//Trebble boost
			//if(f >= settings.trebbleFreq)
				//db += settings.trebbleBoost;

			if(f <= settings.bassFreq)
				energy *= absBoost;

			energy -= prevColor.avg;
			if(energy < 0)
				energy = 0;

			//Raise by noise floor, subtract loosly-tracking average
			//db += settings.noiseFloor;// - prevColor.avg;

			//Reject anything below the average
			//if(db <= prevColor.avg)
				//continue;

			//Scale partially based on average level
			//db *= settings.dbFactor;
			//db += settings.avgFactor*prevColor.avg;

			//Add weighted color to running color average
			r += energy * c.getRed();
			g += energy * c.getGreen();
			b += energy * c.getBlue();
		}
	}

	//Scale color
	r *= r*scale;
	g *= g*scale;
	b *= b*scale;

	//Convert to DB
	r = 20.*std::log10(r);
	if(r < 0)
		r = 0;
	g = 20.*std::log10(g);
	if(g < 0)
		g = 0;
	b = 20.*std::log10(b);
	if(b < 0)
		b = 0;
	
	//Compute largest component
	double largest = std::max(r, std::max(g, b));

	//Use the largest value to limit the maximum brightness to 255
	if(largest > 255) {
		double scale = 255. / largest;

		r *= scale;
		g *= scale;
		b *= scale;
	}

	//std::cout << r << ' ' << g << ' ' << b << std::endl;
	//std::printf("%3d %3d %3d\n", (int)r, (int)g, (int)b);

	Color c(r, g, b);
	double h = c.getHue(), s = c.getHSVSaturation(), v = c.getValue();

	//Enforce saturation minimum
	c = Color::HSV(h, std::max(settings.minSaturation, s), v);

	//Filter the color
	prevColor.c.filter(c, settings.filterStrength);
}
