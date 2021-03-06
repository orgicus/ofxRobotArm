//Copyright (c) 2016, Daniel Moore, Madaline Gannon, and The Frank-Ratchye STUDIO for Creative Inquiry All rights reserved.

//--------------------------------------------------------------
//
//
// Follow Path Example
//
//
//--------------------------------------------------------------

//
// This example shows you how to:
//  1.  Import and Create 2D/3D paths
//  2.  Move & reiorient the robot using a PathController
//  3.  Dynamically move paths using keypressed
//
// See the ReadMe for more tutorial details
//
// TO DO:
//      - Fix robot misaligment to perp planes
//      - Realign default perp plane to local axes

#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(0);
    ofSetLogLevel(OF_LOG_SILENT);
    
    setupViewports();
    
    parameters.setup();
    robot.setup("192.168.1.9",parameters,UR10); // <-- swap with your robot's ip address
    
    setupGUI();
    positionGUI();
    
    // setup path controller
    path.setup();
    line = buildPath();
    // load/create different paths
    path.set(line);
    vector<Path *> pathPtrs;
    pathPtrs.push_back(&path);
    paths.setup(pathPtrs);
    
    feedRate = 0.001;
}

//--------------------------------------------------------------
ofPolyline ofApp::buildPath(){
    
    ofPolyline temp;

    ofNode n0;
    ofNode n1;
    ofNode n2;
    
    n0.setPosition(.5,.25,.25); // all coordinates are in meters);
    n1.setParent(n0);
    n1.setPosition(0,0,.1);
    n2.setParent(n1);
    n2.setPosition(0,.0015,0);
    
    float totalRotation = 0;
    float step = .25;
    while (totalRotation < 360){
        
        n0.pan(step);
        n1.tilt(2);
        n2.roll(1);
        
        ofPoint p = n2.getGlobalPosition().rotate(90, ofVec3f(1,0,0));
        
        // and point to path
        temp.addVertex(p);
        totalRotation += step;
    }
    
    temp.close();
    temp = temp.getResampledBySpacing(feedRate);
    return temp;
}

//--------------------------------------------------------------
void ofApp::update(){
    
    paths.update();
    
    moveArm();
    robot.update();
    
    updateActiveCamera();
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    ofSetColor(255,160);
    ofDrawBitmapString("OF FPS "+ofToString(ofGetFrameRate()), 30, ofGetWindowHeight()-50);
    ofDrawBitmapString("Robot FPS "+ofToString(robot.robot.getThreadFPS()), 30, ofGetWindowHeight()-65);
    
    gizmo.setViewDimensions(viewportSim.width, viewportSim.height);
    
    // show realtime robot
    cams[0]->begin(viewportReal);
    tcpNode.draw();
    paths.draw();
    robot.robot.model.draw();
    cams[0]->end();
    
    // show simulation robot
    cams[1]->begin(viewportSim);
    if (parameters.bFollow)
        gizmo.draw(*cams[1]);
    paths.draw();
    robot.movement.draw(0);
    cams[1]->end();
    
    drawGUI();
    
}

void ofApp::moveArm(){
    
    // assign the target pose to the current robot pose
    if(parameters.bCopy){
        parameters.bCopy = false;
        parameters.targetTCP.rotation = ofQuaternion(90, ofVec3f(0, 0, 1));
        parameters.targetTCP.rotation*=ofQuaternion(90, ofVec3f(1, 0, 0));
        
        // get the robot's position
        parameters.targetTCP.position = parameters.actualTCP.position;
        parameters.targetTCP.rotation*=parameters.actualTCP.rotation;
        
        // update the gizmo controller
        tcpNode.setPosition(parameters.targetTCP.position*1000);
        tcpNode.setOrientation(parameters.targetTCP.rotation);
        gizmo.setNode(tcpNode);
        
        // update GUI params
        parameters.targetTCPPosition = parameters.targetTCP.position;
        parameters.targetTCPOrientation = ofVec4f(parameters.targetTCP.rotation.x(), parameters.targetTCP.rotation.y(), parameters.targetTCP.rotation.z(), parameters.targetTCP.rotation.w());
        
    }
    else{
        // update the tool tcp
        tcpNode.setTransformMatrix(gizmo.getMatrix());
    }

    
    // follow a user-defined position and orientation
    if(parameters.bFollow){
        parameters.targetTCP.position.interpolate(tcpNode.getPosition()/1000.0, parameters.followLerp);
        parameters.targetTCP.rotation = tcpNode.getOrientationQuat();
        parameters.targetTCPOrientation = ofVec4f(parameters.targetTCP.rotation.x(), parameters.targetTCP.rotation.y(), parameters.targetTCP.rotation.z(), parameters.targetTCP.rotation.w());
        
    }
    
    if (parameters.bMove){
        ofMatrix4x4 orientation = paths.getNextPose();
        parameters.targetTCP.position = orientation.getTranslation();
        
        parameters.targetTCP.rotation = ofQuaternion(90, ofVec3f(0, 0, 1));
        parameters.targetTCP.rotation*=ofQuaternion(90, ofVec3f(1, 0, 0));
        parameters.targetTCP.rotation *= orientation.getRotate();
        
        // update the gizmo controller
        tcpNode.setPosition(parameters.targetTCP.position*1000);
        tcpNode.setOrientation(parameters.targetTCP.rotation);
        gizmo.setNode(tcpNode);
    }
}

void ofApp::setupViewports(){
    
    viewportReal = ofRectangle((21*ofGetWindowWidth()/24)/2, 0, (21*ofGetWindowWidth()/24)/2, 8*ofGetWindowHeight()/8);
    viewportSim = ofRectangle(0, 0, (21*ofGetWindowWidth()/24)/2, 8*ofGetWindowHeight()/8);
    
    activeCam = 0;
    
    
    for(int i = 0; i < N_CAMERAS; i++){
        cams.push_back(new ofEasyCam());
        savedCamMats.push_back(ofMatrix4x4());
        viewportLabels.push_back("");
    }
    
    cams[0]->begin(viewportReal);
    cams[0]->end();
    cams[0]->enableMouseInput();
    
    
    cams[1]->begin(viewportSim);
    cams[1]->end();
    cams[1]->enableMouseInput();
    
}

//--------------------------------------------------------------
void ofApp::setupGUI(){
    
    panel.setup(parameters.robotArmParams);
    panel.add(parameters.pathRecorderParams);
    
    panelJoints.setup(parameters.joints);
    panelTargetJoints.setup(parameters.targetJoints);
    panelJointsSpeed.setup(parameters.jointSpeeds);
    panelJointsIK.setup(parameters.jointsIK);
    
    panel.add(robot.movement.movementParams);
    parameters.bMove = false;
    // get the current pose on start up
    parameters.bCopy = true;
    panel.loadFromFile("settings/settings.xml");
    
    // setup Gizmo
    gizmo.setDisplayScale(1.0);
    tcpNode.setPosition(ofVec3f(0.5, 0.5, 0.5)*1000);
    tcpNode.setOrientation(parameters.targetTCP.rotation);
    gizmo.setNode(tcpNode);
}

void ofApp::positionGUI(){
    viewportReal.height -= (panelJoints.getHeight());
    viewportReal.y +=(panelJoints.getHeight());
    panel.setPosition(viewportReal.x+viewportReal.width, 10);
    panelJointsSpeed.setPosition(viewportReal.x, 10);
    panelJointsIK.setPosition(panelJointsSpeed.getPosition().x+panelJoints.getWidth(), 10);
    panelTargetJoints.setPosition(panelJointsIK.getPosition().x+panelJoints.getWidth(), 10);
    panelJoints.setPosition(panelTargetJoints.getPosition().x+panelJoints.getWidth(), 10);
}

//--------------------------------------------------------------
void ofApp::drawGUI(){
    panel.draw();
    panelJoints.draw();
    panelJointsIK.draw();
    panelJointsSpeed.draw();
    panelTargetJoints.draw();
    
    hightlightViewports();
}


//--------------------------------------------------------------
void ofApp::updateActiveCamera(){
    
    if (viewportReal.inside(ofGetMouseX(), ofGetMouseY()))
    {
        activeCam = 0;
        if(!cams[0]->getMouseInputEnabled()){
            cams[0]->enableMouseInput();
        }
        if(cams[1]->getMouseInputEnabled()){
            cams[1]->disableMouseInput();
        }
    }
    if(viewportSim.inside(ofGetMouseX(), ofGetMouseY()))
    {
        activeCam = 1;
        if(!cams[1]->getMouseInputEnabled()){
            cams[1]->enableMouseInput();
        }
        if(cams[0]->getMouseInputEnabled()){
            cams[0]->disableMouseInput();
        }
        if(gizmo.isInteracting() && cams[1]->getMouseInputEnabled()){
            cams[1]->disableMouseInput();
        }
    }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::handleViewportPresets(int key){
    
    float dist = 2000;
    float zOffset = 450;
    
    if(activeCam != -1){
        // TOP VIEW
        if (key == '1'){
            cams[activeCam]->reset();
            cams[activeCam]->setPosition(0, 0, dist);
            cams[activeCam]->lookAt(ofVec3f(0, 0, 0), ofVec3f(0, 0, 1));
            viewportLabels[activeCam] = "TOP VIEW";
        }
        // LEFT VIEW
        else if (key == '2'){
            cams[activeCam]->reset();
            cams[activeCam]->setPosition(dist, 0, 0);
            cams[activeCam]->lookAt(ofVec3f(0, 0, 0), ofVec3f(0, 0, 1));
            viewportLabels[activeCam] = "LEFT VIEW";
        }
        // FRONT VIEW
        else if (key == '3'){
            cams[activeCam]->reset();
            cams[activeCam]->setPosition(0, dist, 0);
            cams[activeCam]->lookAt(ofVec3f(0, 0, 0), ofVec3f(0, 0, 1));
            viewportLabels[activeCam] = "FRONT VIEW";
        }
        // PERSPECTIVE VIEW
        else if (key == '4'){
            cams[activeCam]->reset();
            cams[activeCam]->setPosition(dist, dist, dist/4);
            cams[activeCam]->lookAt(ofVec3f(0, 0, 0), ofVec3f(0, 0, 1));
            viewportLabels[activeCam] = "PERSPECTIVE VIEW";
        }
    }
}


//--------------------------------------------------------------
void ofApp::hightlightViewports(){
    ofPushStyle();
    
    // highlight right viewport
    if (activeCam == 0){
        
        ofSetLineWidth(6);
        ofSetColor(ofColor::white,30);
        ofDrawRectangle(viewportReal);
  
    }
    // hightlight left viewport
    else{
        ofSetLineWidth(6);
        ofSetColor(ofColor::white,30);
        ofDrawRectangle(viewportSim);
    }
    
    // show Viewport info
    ofSetColor(ofColor::white,200);
    ofDrawBitmapString(viewportLabels[0], viewportReal.x+viewportReal.width-120, ofGetWindowHeight()-30);
    ofDrawBitmapString("REALTIME", ofGetWindowWidth()/2 - 90, ofGetWindowHeight()-30);
    ofDrawBitmapString(viewportLabels[1], viewportSim.x+viewportSim.width-120, ofGetWindowHeight()-30);
    ofDrawBitmapString("SIMULATED", 30, ofGetWindowHeight()-30);
    
    ofPopStyle();
}


//--------------------------------------------------------------
void ofApp::keyPressed(int key){

    if(key == 'm'){
        parameters.bMove = !parameters.bMove;
        
        if (parameters.bMove)
            paths.startDrawing();
        else
            paths.pauseDrawing();
    }
    
    if( key == 'r' ) {
        gizmo.setType( ofxGizmo::OFX_GIZMO_ROTATE );
    }
    if( key == 'g' ) {
        gizmo.setType( ofxGizmo::OFX_GIZMO_MOVE );
    }
    if( key == 's' ) {
        gizmo.setType( ofxGizmo::OFX_GIZMO_SCALE );
    }
    if( key == 'e' ) {
        gizmo.toggleVisible();
    }
    if(key == ' '){
        feedRate+=0.001;
        if(feedRate > 0.01){
            feedRate = 0.001;
        }
        line = buildPath();
        path.set(line);
        startFrameNum = ofGetFrameNum();
    }
    paths.keyPressed(key);

    handleViewportPresets(key);
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    viewportLabels[activeCam] = "";
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
