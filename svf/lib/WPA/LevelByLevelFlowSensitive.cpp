

#include "WPA/LevelByLevelFlowSensitive.h"
#include "WPA/WPAStat.h"

// This cannot goes into header. Causing recursive include.
#include "WPA/Steensgaard.h"


using namespace SVF;
using namespace SVFUtil;


void LevelByLevelFlowSensitive::computePointerLevels(std::map<NodeID, std::set<NodeID>> &dag){
    for(auto it = dag.begin(); it != dag.end(); ++it){
        pointerLevelToNodeIDsMap[getPointerLevel(it->first, dag)].insert(it->first);
    }


}


size_t LevelByLevelFlowSensitive::getPointerLevel(NodeID id, std::map<NodeID, std::set<NodeID>> &dag){
    
    if(!dag.count(id)){
        return 0;
    }
    
    if(pointerLevelMap.count(id)){
        return pointerLevelMap.at(id);
    }
    
    size_t pl = 0;
    for(auto to : dag.at(id)){
        pl = std::max(pl, getPointerLevel(to, dag));
    }

    pointerLevelMap[id] = pl+1;
    return pointerLevelMap[id];

}



void LevelByLevelFlowSensitive::initialize(){
    PointerAnalysis::initialize();

    


    // create stats
    stat = new LevelByLevelFlowSensitiveStat(this);

    steen = Steensgaard::createSteensgaard(getPAG());
    ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());

    


    // ptg = new PointsToGraph(steen);
    ptg = new PointsToGraph(ander);



    pointsToGraphSCCDetection();

    // output some stats first

    // for(auto it = steen->getAllValidPtrs().begin(); it != steen->getAllValidPtrs().end(); ++it){
    //     SVF::NodeID nid = *it;

    //     outs() << "Node id " << nid << " has rep node " << ptgScc->repNode(nid) << "\n";

    // }

    // turn the SCC into DAG
    std::map<NodeID, std::set<NodeID>> dag;

    for(auto it = ander->getAllValidPtrs().begin(); it != ander->getAllValidPtrs().end(); ++it){
        SVF::NodeID nid = *it;
        for(auto pointeeId : ander->getPts(nid)){
            if(ptgScc->repNode(nid) != ptgScc->repNode(pointeeId)){
                dag[ptgScc->repNode(nid)].insert(ptgScc->repNode(pointeeId));
            }
        }
    }

    outs() << "print dag\n";
    for(auto p : dag){
        outs() << p.first << " => \n";
        for(auto pp : p.second){
            outs() << "\t" << pp << "\n";
        }
    }


    // JH todo: currently, the pts in svf does not capture relationship among adt variables. These information needs to be restored by checking store insts.
    computePointerLevels(dag);

    
    for(auto p : pointerLevelMap){
        outs() << p.first << " => " << p.second << "\n";
    }




    

    // compute pointer level from DAG


    

    // outs() << "Printing steengaard result\n";

    // for(auto it = steen->getAllValidPtrs().begin(); it != steen->getAllValidPtrs().end(); ++it){
    //     SVF::NodeID nid = *it;
    //     outs() << nid << "\n";
    //     for(auto pointeeId : steen->getPts(nid)){
    //         // auto pointeeNode = steenPta->getGNode(pointeeId);
    //         outs() << "\t=> " << pointeeId << "\n"; 
    //     }
    // }


    // outs() << "Printing andersen result\n";
    // // steen = Steensgaard::createSteensgaard(getPAG());

    // for(auto it = ander->getAllValidPtrs().begin(); it != ander->getAllValidPtrs().end(); ++it){
    //     SVF::NodeID nid = *it;
    //     outs() << nid << "\n";
    //     for(auto pointeeId : ander->getPts(nid)){
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

    // outs() << "Building points-to graph.\n";

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