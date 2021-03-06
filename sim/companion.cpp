/*
   companion.cpp : Companion-board class implementation

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "companion.hpp"

#if defined(__linux) && defined(_COMPANION)

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <netdb.h>
using namespace cv;

#include <iostream>
using namespace std;

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h> 
#endif

#if defined(__linux) && defined(_COMPANION)
static int connect_to_server(int port, const char * hostname="localhost")
{
    // http://web.eecs.utk.edu/~plank/plank/classes/cs360/360/notes/Sockets/sockettome.c
    struct sockaddr_in sn;
    struct hostent *he;
    if (!(he = gethostbyname(hostname))) {
        printf("can't get host id for %s\n", hostname);
    }
    int ok = 0;
    int sockfd = 0;
    while (!ok) {
        sn.sin_family = AF_INET;
        sn.sin_port  = htons(port);
        sn.sin_addr.s_addr = *(u_long*)(he->h_addr_list[0]);

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket()");
        }
        ok = (connect(sockfd, (struct sockaddr*)&sn, sizeof(sn)) != -1);
        if (!ok) sleep (1);
    }  
    return sockfd;
}

static const int CAMERA_PORT       = 5000;
static const int COMMS_PORT        = 5001;
static const char * IMAGE_FILENAME = "image.jpg";
static const int MAXMSG            = 1000;

#endif

CompanionBoard::CompanionBoard(void)
{
    this->procid = 0;
}

void CompanionBoard::start(void)
{
#if defined(__linux) && defined(_COMPANION)

    // Build command-line arguments for forking the Python server script
    char script[200];
    sprintf(script, "%s/hackflight_companion.py", VREP_DIR);
    char camera_port[10];
    sprintf(camera_port, "%d", CAMERA_PORT);
    char comms_port[10];
    sprintf(comms_port, "%d", COMMS_PORT);
    char *argv[5] = {(char *)script, camera_port, comms_port, (char *)IMAGE_FILENAME, NULL};

    // Fork the Python server script
    this->procid = fork();
    if (this->procid == 0) {
        execvp(script, argv);
        exit(0);
    }

    // Open a socket for syncing camera images with the server, and a socket for comms
    this->camera_sockfd = connect_to_server(CAMERA_PORT);
    this->comms_sockfd = connect_to_server(COMMS_PORT);

#endif
}

void CompanionBoard::update(char * imageBytes, int imageWidth, int imageHeight)
{
#if defined(__linux) && defined(_COMPANION)

    // Use OpenCV to convert BGR image bytes to RGB, and save as JPEG
    Mat image = Mat(imageHeight, imageWidth, CV_8UC3, imageBytes);
    flip(image, image, 0);                 // rectify image
    cvtColor(image, image, COLOR_BGR2RGB); // convert image BGR->RGB
    imwrite(IMAGE_FILENAME, image);

    // Send sync byte to Python client, which will open and process the image
    char sync = 0;
    write(this->camera_sockfd, &sync, 1);

    // Check whether bytes are available from server
    int avail;
    ioctl(this->comms_sockfd, FIONREAD, &avail);

    // Ignore OOB values
    if (avail > 0 && avail < MAXMSG) {
        char msg[MAXMSG];
        read(this->comms_sockfd, msg, avail);
    }

#endif
}

void CompanionBoard::halt(void)
{
#if defined(__linux) && defined(_COMPANION)
    if (this->procid) {
        close(this->camera_sockfd);
        close(this->comms_sockfd);
        kill(this->procid, SIGKILL);
    }
#endif
}
