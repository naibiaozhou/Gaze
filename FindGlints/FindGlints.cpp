#include <iostream>
#include <math.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
// TODO: entfernen
#include "opencv2/highgui/highgui.hpp"

#include "FindGlints.hpp"
#include "utils/log.hpp"
#include "utils/geometry.hpp"
#include "GazeConstants.hpp"

using namespace std;
using namespace cv;

bool FindGlints::findGlints(cv::Mat& frame, vector<cv::Point>& glintCenter) {

	MatND hist;
	Mat img;

	// TODO put variables in setUp
	int nbins = 256; // 256 levels histogram levels
	int hsize[] = { nbins }; // just one dimension
	float range[] = { 0, 255 };
	const float *ranges[] = { range };
	int chnls[] = { 0 };

	// Calc history
	calcHist(&frame, 1, chnls, Mat(), hist, 1, hsize, ranges);

	float pixelSum = 0;
	int threshold = 255;

	// Estimated diameter of pupil
	int diameter = 4;
	// Estimated number of pixels for two pupils
	int pixelSize = diameter * diameter * 3.14 * 8;

	// Find threshold beginning with brightest point
	for (int i = hist.rows; i > 0; i--) {
		float histValue = hist.at<float>(i, 0);
		pixelSum += histValue;
		if (pixelSum > pixelSize) {
			threshold = i;
			break;
		}
	}

	// Threshold image.
	cv::threshold(frame, img, threshold, 255, cv::THRESH_TOZERO);

	imshow("Thresholded image ", img);

	cout << "Threshold: " << threshold << endl;

	std::vector<std::vector<cv::Point> > contours;

	// Find all countours
	findContours(img, contours, // a vector of contours
			CV_RETR_EXTERNAL, // retrieve the external contours
			CV_CHAIN_APPROX_NONE); // all pixels of each contours

	LOG_D("Countours size: " << contours.size());

	vector<Vec4i> hierarchy;

	// TODO Add Shape analyses to make sure its really a circular form?
	for (int i = 0; i < contours.size(); i++) {

		// TODO:
		// Filter:
		// 1. nach Grösse -> grosse Objekte ausschliessen
		// 2. nach Rundheit -> unrunde objekte ausschliessen
		// 3. nach Position
		// 4. vorheriger Glint Position
		Moments m = moments(contours[i], false);
		int centerX = m.m10 / m.m00;
		int centerY = m.m01 / m.m00;

		// TODO min & max size
		if (m.m00 > 0 && m.m00 < 10) { // Grösse
			cout << "Pixel size: " << m.m00 << endl;

			// Check if object is circular
			if (centerX > 0 && centerY > 0) {
				// TODO: Add offset for haar part
				Point p(centerX, centerY);

				glintCenter.push_back(p);

				drawContours(img, contours, i, Scalar(255, 255, 255), 2, 8,
						hierarchy, 0, Point());
			}
		}

	}
	Mat a;
	distanceMatrix(a, glintCenter);

	/// Show in a window
	namedWindow("Contours", CV_WINDOW_AUTOSIZE);
	imshow("Contours", img);

	return true;

}

cv::Mat FindGlints::distanceMatrix(cv::Mat & image,
		vector<cv::Point>& glintCenter) {
	// Create identity matrix

	// Amount of glints
	int n = glintCenter.size();

	// Create identity matrix
	Mat distanceMat = Mat::eye(n, n, CV_8U);

	for (int i = 0; i < n; i++) {
		for (int j = i; j < n; j++) {
			int dist = calcPointDistance(glintCenter.at(i), glintCenter.at(j));

			if (dist >= 5
					&& dist <= 50) {
				distanceMat.at< short >(0,0) = 1;
			}
		}
	}

	return image;

}
