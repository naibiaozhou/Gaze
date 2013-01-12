/*
 * GazeConstants.cpp
 *
 *  Created on: Nov 9, 2012
 *      Author: fri
 */

#include "GazeConfig.hpp"

#include <stdlib.h>
#include <unistd.h>

using namespace std;



//
// Glints Configuration
//
int GazeConfig::GLINT_THRESHOLD;
int GazeConfig::GLINT_DISTANCE_TOLERANCE;
int GazeConfig::GLINT_DISTANCE;
int GazeConfig::GLINT_ANGLE_TOLERANCE;
//
// Haar configuration
//

int GazeConfig::HAAR_EYEREGION_MIN_HEIGHT;
int GazeConfig::HAAR_EYEREGION_MIN_WIDTH;
int GazeConfig::HAAR_FINDREGION_MAX_TRIES;

//
//  General settings
//
bool GazeConfig::DETECT_LEFT_EYE; 

string GazeConfig::inHomeDirectory(string suffix) {
	string home(getenv("HOME"));
	return home + "/" + suffix;
}

string GazeConfig::inWorkingDir(std::string suffix){
    long size;
    char *buf;
    char *ptr;

    size = pathconf(".", _PC_PATH_MAX);

    if ((buf = (char *)malloc((size_t)size)) != NULL)
        ptr = getcwd(buf, (size_t)size);
    else
        return suffix;

    string s(ptr);
    return s + "/" + suffix;
}
