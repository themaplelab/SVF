

#include "WPA/LevelByLevelFlowSensitive.h"
#include "WPA/WPAStat.h"

// This cannot goes into header. Causing recursive include.
#include "WPA/Steensgaard.h"


using namespace SVF;
using namespace SVFUtil;



void LevelByLevelFlowSensitive::initialize(){
    PointerAnalysis::initialize();

    


    // create stats
    stat = new LevelByLevelFlowSensitiveStat(this);

    steen = Steensgaard::createSteensgaard(getPAG());
    


    ptg = new PointsToGraph(steen);


    pointsToGraphSCCDetection();


    

    // outs() << "Printing steengaard result\n";

    // for(auto it = steen->getAllValidPtrs().begin(); it != steen->getAllValidPtrs().end(); ++it){
    //     SVF::NodeID nid = *it;
    //     outs() << nid << "\n";
    //     for(auto pointeeId : steen->getPts(nid)){
    //         // auto pointeeNode = steenPta->getGNode(pointeeId);
    //         outs() << "\t=> " << pointeeId << "\n"; 
    //     }
    // }

    /*
        node id to node: "const PAGNode* node = getPAG()->getGNode(*nIter);"
        node to nodeid: "node->getId();"
        traversal pointers: "for(auto it = steen->getAllValidPtrs().begin(); it != steen->getAllValidPtrs().end(); ++it)"
        test top level valid pointers: "steen->getPAG()->isValidTopLevelPtr(node)"
        get pts: "steen->getPts(nid)"
    */

    outs() << "Building points-to graph.\n";

    // todo: which object we use to represent a points-to graph?
    // Figure out how to create a new graph class that subclass generalGraph that represents points-to graph.


}



void LevelByLevelFlowSensitive::analyze(){

    outs() << "Levpa analyze.\n";

    initialize();

    std::terminate();



    // if(Options::WriteAnder().empty()){
    //     // initialize - split pointers into groups

    //     // while not final layer
    //     // build mssa and svfg for current layer

    //     // solve fixed point for current layer

    //     // cleanup


    // }
}