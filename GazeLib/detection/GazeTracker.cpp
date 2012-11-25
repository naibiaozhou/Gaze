/*
 * GazeTracker.cpp
 *
 *  Created on: Nov 24, 2012
 *      Author: krigu
 */

#include "GazeTracker.hpp"
#include "../utils/geometry.hpp"
#include "../utils/gui.hpp"
#include "../GazeConstants.hpp"
#include "../utils/log.hpp"

// TODO: remove
#include "opencv2/highgui/highgui.hpp"

using namespace cv;

GazeTracker::GazeTracker(ImageSource & imageSource) :
		imageSrc(imageSource), isRunning(false), isStopping(false) {
}

bool GazeTracker::initialize(cv::Mat& frame, cv::Rect& frameRegion,
		cv::Point& frameCenter) {

	bool foundEye = false;
	short tries = 0;
	while (!foundEye) {
		foundEye = eyeFinder.findLeftEye(frame, frameRegion);
		tries++;

		if (tries > GazeConstants::HAAR_FINDREGION_MAX_TRIES)
			return false;
	}

	frameCenter = calcRectBarycenter(frameRegion);
	frame = frame(frameRegion);

	return true;
}

void GazeTracker::adjustRect(cv::Point& currentCenter, cv::Rect& frameRegion) {
	int width = frameRegion.width;
	int height = frameRegion.height;

	int x = frameRegion.x + currentCenter.x - (width / 2);
	int y = frameRegion.y + currentCenter.y - (height / 2);

	// Range check
	x = (x > 0) ? x : 0;
	y = (y > 0) ? y : 0;

	frameRegion = Rect(x, y, width, height);
}

GazeTracker::~GazeTracker() {
	// TODO Auto-generated destructor stub
}

bool GazeTracker::startTracking() {

	Mat currentFrame;
	Rect frameRegion;
	Point frameCenter;
	vector<cv::Point> glints;

	bool hasImage = imageSrc.nextGrayFrame(currentFrame);
	// TODO: return error?
	if (!hasImage) {
		LOG_D("No image");
		return false;
	}

	bool foundRegion = initialize(currentFrame, frameRegion, frameCenter);
	if (!foundRegion) {
		LOG_D("No region found");
		return false;
	}

	LOG_D("frameCenter:" << frameCenter <<  "x: " << frameRegion.x << "y: " << frameRegion.y);

	isRunning = true;
	int noGlints = 0;
	// main loop
	while (true) {
		// Get next frame
		if (!imageSrc.nextGrayFrame(currentFrame)){
			LOG_D("No more frames");
			break;
		}
		// TODO rename
//#ifdef __DEBUG_FIND_GLINTS
		Mat orig = currentFrame.clone();
		rectangle(orig, frameRegion, Scalar(255,255,255), 1);
		imshow("Search region", orig);
//#endif



		// Adjust rect
		currentFrame = currentFrame(frameRegion);

		if (glintFinder.findGlints(currentFrame, glints, frameCenter)) {
			LOG_D("frameCenter:" << frameCenter <<  "x: " << frameRegion.x << "y: " << frameRegion.y);
			adjustRect(frameCenter, frameRegion);
		} else {
			noGlints++;
			if (noGlints > 5) {
				imageSrc.nextGrayFrame(currentFrame);
				initialize(currentFrame, frameRegion, frameCenter);

				noGlints = 0;
			}
		}
		cross(currentFrame, frameCenter,5);
		imshow("After calib", currentFrame);

		// check the key and add some busy waiting
		int keycode = waitKey(100);
		if (keycode == 32) // space
			while (waitKey(100) != 32)
				;

		// TODO: check if it works
		while (!isRunning) {
			sleep(1);
		}

		if (isStopping)
			break;
	}
	return true;
}

void GazeTracker::pauseTacking() {
	isRunning = false;
}

void GazeTracker::stopTracking() {
	isStopping = true;
}