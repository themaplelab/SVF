
#include "Graphs/PointsToGraph.h"
#include "WPA/Steensgaard.h"
#include "SVFIR/SVFIR.h"




using namespace SVF;
using namespace SVFUtil;




PointsToGraph::PointsToGraph(Steensgaard *steen){

    // auto svfir = steen->getPAG();

    for(auto it = steen->getAllValidPtrs().begin(); it != steen->getAllValidPtrs().end(); ++it){

        SVF::NodeID pointerId = *it;
        // auto PointerNode = svfir->getGNode(pointerId);
        // add node
        if(!hasGNode(pointerId)){
            PointsToGraphNode *ptgNode = new PointsToGraphNode(pointerId);
            addGNode(pointerId, ptgNode);
        }

        auto pointerNode = getGNode(pointerId);


        for(auto pointeeId : steen->getPts(pointerId)){
            
            //check if node already exist
            // if not, add node and edge
            if(!hasGNode(pointeeId)){
                PointsToGraphNode *ptgNode = new PointsToGraphNode(pointeeId);
                addGNode(pointeeId, ptgNode);
                auto pointeeNode = getGNode(pointeeId);
                PointsToGraphEdge* newEdge = new PointsToGraphEdge(pointerNode, pointeeNode, PointsToGraphEdge::PtEDGEK::PointsTo);
                addEdge(newEdge);
            }
            else{
                auto pointeeNode = getGNode(pointeeId);
                PointsToGraphEdge* newEdge = new PointsToGraphEdge(pointerNode, pointeeNode, PointsToGraphEdge::PtEDGEK::PointsTo);
                addEdge(newEdge);
            }
        }

    }
}
