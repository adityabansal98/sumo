/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEDetectorE2.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Nov 2015
/// @version $Id$
///
//
/****************************************************************************/

// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <string>
#include <iostream>
#include <utility>
#include <netbuild/NBEdge.h>
#include <utils/geom/PositionVector.h>
#include <utils/common/RandHelper.h>
#include <utils/common/SUMOVehicleClass.h>
#include <utils/common/ToString.h>
#include <utils/geom/GeomHelper.h>
#include <utils/gui/windows/GUISUMOAbstractView.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/images/GUITextureSubSys.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/images/GUITexturesHelper.h>
#include <utils/xml/SUMOSAXHandler.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/netelements/GNEConnection.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNENet.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/GNEViewParent.h>

#include "GNEDetectorE2.h"


// ===========================================================================
// member method definitions
// ===========================================================================

GNEDetectorE2::GNEDetectorE2(const std::string& id, GNELane* lane, GNEViewNet* viewNet, double pos, double length, double freq, const std::string& filename, const std::string& vehicleTypes,
                             const std::string& name, const double timeThreshold, double speedThreshold, double jamThreshold, bool friendlyPos, bool blockMovement) :
    GNEDetector(id, viewNet, GLO_E2DETECTOR, SUMO_TAG_E2DETECTOR, pos, freq, filename, vehicleTypes, name, friendlyPos, blockMovement),
    myLanes({lane}),
    myEndPositionOverLane(0), 
    myLength(length),
    myTimeThreshold(timeThreshold),
    mySpeedThreshold(speedThreshold),
    myJamThreshold(jamThreshold) {
}


GNEDetectorE2::GNEDetectorE2(const std::string& id, std::vector<GNELane*> lanes, GNEViewNet* viewNet, double pos, double endPos, double freq, const std::string& filename, const std::string& vehicleTypes,
                             const std::string& name, const double timeThreshold, double speedThreshold, double jamThreshold, bool friendlyPos, bool blockMovement) :
    GNEDetector(id, viewNet, GLO_E2DETECTOR, SUMO_TAG_E2DETECTOR_MULTILANE, pos, freq, filename, vehicleTypes, name, friendlyPos, blockMovement),
    myLanes(lanes),
    myEndPositionOverLane(endPos),
    myTimeThreshold(timeThreshold),
    mySpeedThreshold(speedThreshold),
    myJamThreshold(jamThreshold) {
}


GNEDetectorE2::~GNEDetectorE2() {
}


void
GNEDetectorE2::updateGeometry(bool updateGrid) {
    // first check if object has to be removed from grid (SUMOTree)
    if (updateGrid) {
        myViewNet->getNet()->removeGLObjectFromGrid(this);
    }

    // Clear all containers for single-shape elements
    myShapeRotations.clear();
    myShapeLengths.clear();
    myShape.clear();

    // clear all containers for multi-shape elements 
    myMultiShapeRotations.clear();
    myMultiShapeLengths.clear();
    myMultiShape.clear();

    // declare variables for start and end positions
    double startPosFixed, endPosFixed;

    // calculate start and end positions dependin of number of lanes
    if (myLanes.size() == 1) {
        // set shape lane as detector shape
        myShape = myLanes.front()->getShape();

        // set start position
        if (myPositionOverLane < 0) {
            startPosFixed = 0;
        } else if (myPositionOverLane > myLanes.back()->getParentEdge().getNBEdge()->getFinalLength()) {
            startPosFixed = myLanes.back()->getParentEdge().getNBEdge()->getFinalLength();
        } else {
            startPosFixed = myPositionOverLane;
        }

        // set end position
        if ((myPositionOverLane + myLength) < 0) {
            endPosFixed = 0;
        } else if ((myPositionOverLane + myLength) > myLanes.back()->getParentEdge().getNBEdge()->getFinalLength()) {
            endPosFixed = myLanes.back()->getParentEdge().getNBEdge()->getFinalLength();
        } else {
            endPosFixed = (myPositionOverLane + myLength);
        }
        
        // Cut shape using as delimitators fixed start position and fixed end position
        myShape = myShape.getSubpart(startPosFixed * myLanes.front()->getLengthGeometryFactor(), endPosFixed * myLanes.back()->getLengthGeometryFactor());
    
        // Get number of parts of the shape
        int numberOfSegments = (int)myShape.size() - 1;

        // If number of segments is more than 0
        if (numberOfSegments >= 0) {

            // Reserve memory (To improve efficiency)
            myShapeRotations.reserve(numberOfSegments);
            myShapeLengths.reserve(numberOfSegments);

            // For every part of the shape
            for (int i = 0; i < numberOfSegments; ++i) {

                // Obtain first position
                const Position& f = myShape[i];

                // Obtain next position
                const Position& s = myShape[i + 1];

                // Save distance between position into myShapeLengths
                myShapeLengths.push_back(f.distanceTo(s));

                // Save rotation (angle) of the vector constructed by points f and s
                myShapeRotations.push_back((double)atan2((s.x() - f.x()), (f.y() - s.y())) * (double) 180.0 / (double)M_PI);
            }
        }

        // Set block icon position
        myBlockIconPosition = myShape.getLineCenter();

    } else if (myLanes.size() > 1) {
        // start with the first lane shape
        myMultiShape.push_back(myLanes.front()->getShape());
       
        // set start position
        if (myPositionOverLane < 0) {
            startPosFixed = 0;
        } else if (myPositionOverLane > myLanes.front()->getParentEdge().getNBEdge()->getFinalLength()) {
            startPosFixed = myLanes.front()->getParentEdge().getNBEdge()->getFinalLength();
        } else {
            startPosFixed = myPositionOverLane;
        }
        // Cut shape using as delimitators fixed start position and fixed end position
        myMultiShape[0] = myMultiShape[0].getSubpart(startPosFixed * myLanes.front()->getLengthGeometryFactor(), myLanes.front()->getParentEdge().getNBEdge()->getFinalLength());
       
        // declare last shape
        PositionVector lastShape = myLanes.back()->getShape();

        // set end position
        if (myPositionOverLane < 0) {
            endPosFixed = 0;
        } else if (myPositionOverLane > myLanes.back()->getParentEdge().getNBEdge()->getFinalLength()) {
            endPosFixed = myLanes.back()->getParentEdge().getNBEdge()->getFinalLength();
        } else {
            endPosFixed = myPositionOverLane;
        }

        // Cut shape using as delimitators fixed start position and fixed end position
        lastShape = lastShape.getSubpart(0, endPosFixed * myLanes.back()->getLengthGeometryFactor());

        // add first shape connection (if exist, in other case leave it empty)
        myMultiShape.push_back(PositionVector());
        for (auto j : myLanes.at(0)->getParentEdge().getGNEConnections()) {
            if (j->getLaneTo() == myLanes.at(1)) {
                myMultiShape.back() = j->getShape();
            }
        }

        // append shapes of intermediate lanes AND connections (if exist)
        for (int i = 1; i < (myLanes.size() - 1); i++) {
            // add lane shape
            myMultiShape.push_back(myLanes.at(i)->getShape());
            // add empty shape for connection
            myMultiShape.push_back(PositionVector());
            // set connection shape (if exist). In other case, insert an empty shape
            for (auto j : myLanes.at(i)->getParentEdge().getGNEConnections()) {
                if (j->getLaneTo() == myLanes.at(i+1)) {
                    myMultiShape.back() = j->getShape();
                }
            }
        }

        // append last shape
        myMultiShape.push_back(lastShape);

        // Get number of parts of the shape
        std::vector<int> numberOfSegments;
        for (auto i : myMultiShape) {
            // numseg cannot be 0
            int numSeg = (int)i.size() - 1;
            numberOfSegments.push_back((numSeg>=0)? numSeg : 0);
            myMultiShapeRotations.push_back(std::vector<double>());
            myMultiShapeLengths.push_back(std::vector<double>());
        }

        // If number of segments is more than 0
        for (int i = 0; i < myMultiShape.size(); i++) {
            // Reserve size for every part
            myMultiShapeRotations.back().reserve(numberOfSegments.at(i));
            myMultiShapeLengths.back().reserve(numberOfSegments.at(i));
            // iterate over each segment
            for (int j = 0; j < numberOfSegments.at(i); j++) {

                // Obtain first position
                const Position& f = myMultiShape[i][j];

                // Obtain next position
                const Position& s = myMultiShape[i][j + 1];

                // Save distance between position into myShapeLengths
                myMultiShapeLengths.at(i).push_back(f.distanceTo(s));

                // Save rotation (angle) of the vector constructed by points f and s
                myMultiShapeRotations.at(i).push_back((double)atan2((s.x() - f.x()), (f.y() - s.y())) * (double) 180.0 / (double)M_PI);
            }
        }
        // Set block icon position
        myBlockIconPosition = myMultiShape.front().getLineCenter();
    }

    // Set offset of the block icon
    myBlockIconOffset = Position(-0.75, 0);

    // Set block icon rotation, and using their rotation for draw logo
    setBlockIconRotation(myLanes.front());

    // last step is to check if object has to be added into grid (SUMOTree) again
    if (updateGrid) {
        myViewNet->getNet()->addGLObjectIntoGrid(this);
    }
}


double
GNEDetectorE2::getLength() const {
    return myLength;
}


bool
GNEDetectorE2::isDetectorPositionFixed() const {
    // with friendly position enabled position are "always fixed"
    if (myFriendlyPosition) {
        return true;
    } else {
        return (myPositionOverLane >= 0) && ((myPositionOverLane + myLength) <= myLanes.front()->getParentEdge().getNBEdge()->getFinalLength());
    }
}


GNELane*
GNEDetectorE2::getLane() const {
    return myLanes.front();
}


void
GNEDetectorE2::drawGL(const GUIVisualizationSettings& s) const {
    // Start drawing adding an gl identificator
    glPushName(getGlID());

    // Add a draw matrix
    glPushMatrix();

    // Start with the drawing of the area traslating matrix to origin
    glTranslated(0, 0, getType());

    // Set color of the base
    if (isAttributeCarrierSelected()) {
        GLHelper::setColor(myViewNet->getNet()->selectedAdditionalColor);
    } else {
        GLHelper::setColor(RGBColor(0, 204, 204));
    }

    // Obtain exaggeration of the draw
    const double exaggeration = s.addSize.getExaggeration(s);

    // check if we have to drawn a E2 single lane or a E2 multiLane
    if(myShape.size() > 0) {
        // Draw the area using shape, shapeRotations, shapeLengths and value of exaggeration
        GLHelper::drawBoxLines(myShape, myShapeRotations, myShapeLengths, exaggeration);
    } else {
        // iterate over multishapes
        for (int i = 0; i < myMultiShape.size(); i++) {
            // don't draw shapes over connections if "show connections" is enabled
            if (!myViewNet->showConnections() || (i%2==0)) {
                GLHelper::drawBoxLines(myMultiShape.at(i), myMultiShapeRotations.at(i), myMultiShapeLengths.at(i), exaggeration);
            }
        }
    }

    // Pop last matrix
    glPopMatrix();

    // Check if the distance is enougth to draw details and isn't being drawn for selecting
    if ((s.scale * exaggeration >= 10) && !s.drawForSelecting) {
        // Push matrix
        glPushMatrix();
        // Traslate to center of detector
        glTranslated(myShape.getLineCenter().x(), myShape.getLineCenter().y(), getType() + 0.1);
        // Rotate depending of myBlockIconRotation
        glRotated(myBlockIconRotation, 0, 0, -1);
        //move to logo position
        glTranslated(-0.75, 0, 0);
        std::string logoName = (myLanes.size() == 1)? "E2" : "E2MultiLane";
        // draw E2 logo
        if (isAttributeCarrierSelected()) {
            GLHelper::drawText(logoName, Position(), .1, 1.5, myViewNet->getNet()->selectionColor);
        } else {
            GLHelper::drawText(logoName, Position(), .1, 1.5, RGBColor::BLACK);
        }
        // pop matrix
        glPopMatrix();

        // Show Lock icon depending of the Edit mode
        drawLockIcon();
    }

    // Draw name if isn't being drawn for selecting
    if (!s.drawForSelecting) {
        drawName(getCenteringBoundary().getCenter(), s.scale, s.addName);
    }
    // check if dotted contour has to be drawn
    if (!s.drawForSelecting && (myViewNet->getACUnderCursor() == this)) {
        if(myShape.size() > 0) {
            GLHelper::drawShapeDottedContour(getType(), myShape, exaggeration);
        } else {
            PositionVector multiShapeFixed;
            for (auto i : myMultiShape) {
                multiShapeFixed.append(i);
            }
            GLHelper::drawShapeDottedContour(getType(), multiShapeFixed, exaggeration);
        }
    }
    // Pop name
    glPopName();
}


std::string
GNEDetectorE2::getAttribute(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_ID:
            return getAdditionalID();
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_LANES:
            return parseIDs(myLanes);
        case SUMO_ATTR_POSITION:
            return toString(myPositionOverLane);
        case SUMO_ATTR_ENDPOS:
            return toString(myPositionOverLane);
        case SUMO_ATTR_FREQUENCY:
            return toString(myFreq);
        case SUMO_ATTR_LENGTH:
            return toString(myLength);
        case SUMO_ATTR_NAME:
            return myAdditionalName;
        case SUMO_ATTR_FILE:
            return myFilename;
        case SUMO_ATTR_VTYPES:
            return myVehicleTypes;
        case SUMO_ATTR_HALTING_TIME_THRESHOLD:
            return toString(myTimeThreshold);
        case SUMO_ATTR_HALTING_SPEED_THRESHOLD:
            return toString(mySpeedThreshold);
        case SUMO_ATTR_JAM_DIST_THRESHOLD:
            return toString(myJamThreshold);
        case SUMO_ATTR_FRIENDLY_POS:
            return toString(myFriendlyPosition);
        case GNE_ATTR_BLOCK_MOVEMENT:
            return toString(myBlockMovement);
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        case GNE_ATTR_GENERIC:
            return getGenericParametersStr();
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNEDetectorE2::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_LANES:
        case SUMO_ATTR_POSITION:
        case SUMO_ATTR_ENDPOS:
        case SUMO_ATTR_FREQUENCY:
        case SUMO_ATTR_LENGTH:
        case SUMO_ATTR_NAME:
        case SUMO_ATTR_FILE:
        case SUMO_ATTR_VTYPES:
        case SUMO_ATTR_HALTING_TIME_THRESHOLD:
        case SUMO_ATTR_HALTING_SPEED_THRESHOLD:
        case SUMO_ATTR_JAM_DIST_THRESHOLD:
        case SUMO_ATTR_FRIENDLY_POS:
        case GNE_ATTR_BLOCK_MOVEMENT:
        case GNE_ATTR_SELECTED:
        case GNE_ATTR_GENERIC:
            undoList->p_add(new GNEChange_Attribute(this, key, value));
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEDetectorE2::isValid(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            return isValidAdditionalID(value);
        case SUMO_ATTR_LANE:
            if (value.empty()) {
                return false;
            } else {
                return canParse<std::vector<GNELane*> >(myViewNet->getNet(), value, false);
            }
        case SUMO_ATTR_LANES:
            if (value.empty()) {
                return false;
            } else if (canParse<std::vector<GNELane*> >(myViewNet->getNet(), value, false)) {
                // check that at least there ist TWO lanes
                return (parse<std::vector<GNELane*> >(myViewNet->getNet(), value).size() > 1);
            } else {
                return false;
            }
        case SUMO_ATTR_POSITION:
            return canParse<double>(value);
        case SUMO_ATTR_ENDPOS:
            return canParse<double>(value);
        case SUMO_ATTR_FREQUENCY:
            return (canParse<double>(value) && (parse<double>(value) >= 0));
        case SUMO_ATTR_LENGTH:
            return (canParse<double>(value) && (parse<double>(value) >= 0));
        case SUMO_ATTR_NAME:
            return SUMOXMLDefinitions::isValidAttribute(value);
        case SUMO_ATTR_FILE:
            return SUMOXMLDefinitions::isValidFilename(value);
        case SUMO_ATTR_VTYPES:
            if (value.empty()) {
                return true;
            } else {
                return SUMOXMLDefinitions::isValidListOfTypeID(value);
            }
        case SUMO_ATTR_HALTING_TIME_THRESHOLD:
            return (canParse<double>(value) && (parse<double>(value) >= 0));
        case SUMO_ATTR_HALTING_SPEED_THRESHOLD:
            return (canParse<double>(value) && (parse<double>(value) >= 0));
        case SUMO_ATTR_JAM_DIST_THRESHOLD:
            return (canParse<double>(value) && (parse<double>(value) >= 0));
        case SUMO_ATTR_FRIENDLY_POS:
            return canParse<bool>(value);
        case GNE_ATTR_BLOCK_MOVEMENT:
            return canParse<bool>(value);
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_GENERIC:
            return isGenericParametersValid(value);
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}

// ===========================================================================
// private
// ===========================================================================

void
GNEDetectorE2::setAttribute(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            changeAdditionalID(value);
            break;
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_LANES:
            myLanes = parse<std::vector<GNELane*> >(myViewNet->getNet(), value);
            break;
        case SUMO_ATTR_POSITION:
            myPositionOverLane = parse<double>(value);
            break;
        case SUMO_ATTR_ENDPOS:
            myEndPositionOverLane = parse<double>(value);
            break;
        case SUMO_ATTR_FREQUENCY:
            myFreq = parse<double>(value);
            break;
        case SUMO_ATTR_LENGTH:
            myLength = parse<double>(value);
            break;
        case SUMO_ATTR_NAME:
            myAdditionalName = value;
            break;
        case SUMO_ATTR_FILE:
            myFilename = value;
            break;
        case SUMO_ATTR_VTYPES:
            myVehicleTypes = value;
            break;
        case SUMO_ATTR_HALTING_TIME_THRESHOLD:
            myTimeThreshold = parse<double>(value);
            break;
        case SUMO_ATTR_HALTING_SPEED_THRESHOLD:
            mySpeedThreshold = parse<double>(value);
            break;
        case SUMO_ATTR_JAM_DIST_THRESHOLD:
            myJamThreshold = parse<double>(value);
            break;
        case SUMO_ATTR_FRIENDLY_POS:
            myFriendlyPosition = parse<bool>(value);
            break;
        case GNE_ATTR_BLOCK_MOVEMENT:
            myBlockMovement = parse<bool>(value);
            break;
        case GNE_ATTR_SELECTED:
            if (parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_GENERIC:
            setGenericParametersStr(value);
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
    // After setting attribute always update Geometry
    updateGeometry(true);
}

/****************************************************************************/
