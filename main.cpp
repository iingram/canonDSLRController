//
//  Control Canon DSLRs with two computers, connected via Ethernet
//  Copyright (C) 2014-2015 Ian Ingram & Reid Mitchell
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "main.h"
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"
#include <iostream>
#include <string.h>

// SERVER STUFF
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

EdsError getFirstCamera(EdsCameraRef *camera);
EdsError EDSCALLBACK handleObjectEvent(EdsObjectEvent event, EdsBaseRef object, EdsVoid * context);
EdsError downloadImage(EdsDirectoryItemRef directoryItem);

void cameraSetUp();
void cameraEndSession();
void cameraLoop();
void startRecording();
void stopRecording();

EdsError err=EDS_ERR_OK;
EdsCameraRef camera=NULL;

bool isRecording = false;
bool isSDKloaded = false;
int recordCounter = 0;

void dostuff(int);
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, const char * argv[]) {
    
    cameraLoop();
    //cameraSetUp();
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    

    printf("enter port number: ");
    cin >> portno;
    
    if (portno < 2) {
        fprintf(stderr,"ERROR, bad port provided\n");
        exit(1);
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
     
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (::bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
    listen(sockfd,5);
    
    char reid[100];
    gethostname(reid,sizeof(reid));
    printf("listening on port: %i\nhostname: %s\n",portno,reid);
    
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            error("ERROR on accept");
        } else {
            int n;
            char buffer[256];
            bzero(buffer,256);
            n = read(newsockfd,buffer,255);
            if (n < 0) {
                error("ERROR reading from socket");
            } else if (n==0) {
                //do nothing
            } else {
                string command = string(buffer);

                command = command.substr(0,command.length()-1);
                if(command.compare("start")==0) {
                    startRecording();
                    //cout << "chkpt: recording started" << endl;
                } else if(command.compare("stop")==0) {
                    stopRecording();
                    //cout << "chkpt: recording stopped" << endl;
                } else if(command.compare("quit")==0) {
                    n = write(newsockfd,"command received",18);
                    close (sockfd);
                    cameraEndSession();
                    return 0;
                } else {
                    printf("invalid command\n");
                }
                
                n = write(newsockfd,"command received",18);
                if (n < 0) error("ERROR writing to socket");
            }
        }
    } /* end of while */
    close(sockfd);
    
    cameraEndSession();
    
    return 0;
}

void cameraSetUp() {
    err=EdsInitializeSDK();
    
    if(err==EDS_ERR_OK) isSDKloaded=true;
    if(err==EDS_ERR_OK) err=getFirstCamera(&camera);
    
    if(err==EDS_ERR_OK) {
        err = EdsSetObjectEventHandler(camera, kEdsObjectEvent_All, handleObjectEvent, NULL);
    } else { cout << "problem 1" << endl; }
    
    EdsOpenSession(camera);
    
    EdsInt32 saveTarget = kEdsSaveTo_Camera;
    if(err==EDS_ERR_OK) {
        err = EdsSetPropertyData( camera, kEdsPropID_SaveTo, 0, 4, &saveTarget );
    } else { cout << "problem 2" << endl; }
}

void cameraEndSession() {
    EdsCloseSession(camera);
    EdsTerminateSDK();
    
    //open another session to download all the files created in the last session, then close for good
    cameraSetUp();
    cameraEndSession();
}

void cameraLoop() {

    cameraSetUp();
    
    while(true) {
        string command;
        cout << "go? " << endl;
        cin >> command;
        if(command=="go") {
            break;
        }
    }
    
    for(int i=0; i<60; i++) {

        int pauseTimeInMins = 20;
        int pauseTimeInSecs = 55;//60 * pauseTimeInMins;
        int shotDurationInSecs = 5;
        
        while(pauseTimeInSecs>0) {
            sleep(1);
            cout << "waiting: " << pauseTimeInSecs << " seconds left" << endl;
            pauseTimeInSecs--;
        }
        if(err==EDS_ERR_OK) {
            cout << "starting" << endl;
            startRecording();
            sleep(shotDurationInSecs);
            cout << "stopping" << endl;
            stopRecording();
        } else { cout << "uh oh: " << err << endl; };
        cout << "record count: " << recordCounter << endl;
    }
    
    //cameraEndSession();

}

void startRecording() {
    isRecording = true;
    EdsUInt32 record_start = 4;
    cout << "current kEdsPropID_Record setting: " << kEdsPropID_Record << endl;
    err = EdsSetPropertyData(camera, kEdsPropID_Record, 0, sizeof(record_start), &record_start);
    if(err == EDS_ERR_OK) {
        cout << "recording started" << endl;
        recordCounter++;
    } else {
        cout << "error starting recording" << endl;
    }
}

void stopRecording() {
    isRecording = false;
    EdsUInt32 record_stop = 0;
    cout << "current kEdsPropID_Record setting: " << kEdsPropID_Record << endl;
    err = EdsSetPropertyData(camera, kEdsPropID_Record, 0, sizeof(record_stop), &record_stop);
    if(err == EDS_ERR_OK)
        cout << "recording stopped" << endl;
    else
        cout << "error stopping recording" << endl;
}

EdsError getFirstCamera(EdsCameraRef *camera) {
    EdsError err=EDS_ERR_OK;
    EdsCameraListRef cameraList=NULL;
    EdsUInt32 count=0;
    // Get camera list
    err = EdsGetCameraList(&cameraList);
    // Get number of cameras
    if(err == EDS_ERR_OK) {
        err = EdsGetChildCount(cameraList, &count);
        if(count == 0) {
            err = EDS_ERR_DEVICE_NOT_FOUND;
        }
    }
    // Get first camera retrieved
    if(err == EDS_ERR_OK) {
        err = EdsGetChildAtIndex(cameraList , 0 , camera);
    }
    // Release camera list
    if(cameraList != NULL) {
        EdsRelease(cameraList);
        cameraList = NULL;
    }
    return err;
}

EdsError EDSCALLBACK handleObjectEvent (EdsObjectEvent event, EdsBaseRef object, EdsVoid * context) {
    EdsDirectoryItemInfo objectInfo;
    EdsError err=EDS_ERR_OK;

    switch(event) {
        case kEdsObjectEvent_DirItemCreated:
            err = EdsGetDirectoryItemInfo(object, &objectInfo);
            //downloadImage(object);
            break;
        default:
            break;
    }
    //Release object
    if(object) {
        EdsRelease(object);
    }
    return err;
}

EdsError downloadImage(EdsDirectoryItemRef directoryItem) {
    cout << "Downloading..." << endl;
    EdsError err = EDS_ERR_OK;
    EdsStreamRef stream = NULL;

    // Get directory item information
    EdsDirectoryItemInfo dirItemInfo;
    err = EdsGetDirectoryItemInfo(directoryItem, & dirItemInfo);

    // SET THE DIRECTORY FOR SAVED VIDEO FILES
    EdsChar fileRoot[] = "/Users/reidmitchell/Documents/INGRAM/canonSDKtest2/video/";
    EdsChar filePath[100];
    
    if(err == EDS_ERR_OK) {
        strcpy(filePath, fileRoot);
        strcat(filePath, dirItemInfo.szFileName);
        err = EdsCreateFileStream( filePath, kEdsFileCreateDisposition_CreateAlways,kEdsAccess_ReadWrite, &stream);
    } else { cout << "problem X: " << err << endl; };

    // Download image
    if(err == EDS_ERR_OK) {
        err = EdsDownload( directoryItem, dirItemInfo.size, stream);
    } else { cout << "problem Y: " << err << endl; };

    // Issue notification that download is complete
    if(err == EDS_ERR_OK) {
        err = EdsDownloadComplete(directoryItem);
    } else { cout << "problem Z: " << err << endl; };

    if (stream!=NULL) {
        EdsRelease(stream);
        stream = NULL;
    }
    cout << "Downloaded: " << filePath << endl;
    return err;
}