#include "WPA/LevelByLevelFlowSensitiveOptimized.h"
#include "WPA/WPAStat.h"
#include "WPA/Steensgaard.h"


using namespace SVF;
using namespace SVFUtil;

void LevelByLevelFlowSensitiveOptimized::initialize(){
    PointerAnalysis::initialize();

    
    // create stats
    stat = new LevelByLevelFlowSensitiveStat(this);

    // JH todo: steen seems like does not support inter-procedural analysis for now.
    steen = Steensgaard::createSteensgaard(getPAG());
    ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());
    // ptg = new PointsToGraph(steen);
    ptg = new PointsToGraph(ander);


    pointsToGraphSCCDetection();
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
    currentPointerLevel = computeMaxPointerLevel(dag);



    svfg = memSSA.buildPTROnlySvfgForPointerLevel(this, currentPointerLevel, pointerLevelMap);
    setGraph(svfg);

    svfg->dump("svfg-pl" + std::to_string(currentPointerLevel), true);
    outs() << "End of initialization\n";


    // std::terminate();
}


void LevelByLevelFlowSensitiveOptimized::solveConstraints(){

    bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::FsTimeLimit());
    double start = stat->getClk(true);

    while(true){
        outs() << "opt Solving pointer level " << currentPointerLevel << "\n";
        do
        {
            numOfIteration++;

            if(0 == numOfIteration % OnTheFlyIterBudgetForStat)
                dumpStat();

            callGraphSCC->find();
            initWorklist();
            solveWorklist();

        }
        while (updateCallGraph(getIndirectCallsites()));
        
        --currentPointerLevel;
        if(currentPointerLevel == 0){
            break;
        }
        // JH todo: this should be updating svfg with special handling to intermediate variables.
        memSSA.updatePTROnlySvfgForPointerLevelOptimized(this, currentPointerLevel, pointerLevelMap);
        svfg->dump("svfg-pl" + std::to_string(currentPointerLevel), true);
    }

    SVFUtil::stopAnalysisLimitTimer(limitTimerSet);
    double end = stat->getClk(true);
    solveTime += (end - start) / TIMEINTERVAL;
}


bool LevelByLevelFlowSensitiveOptimized::processLoad(const LoadSVFGNode* load)
{

    outs() << "opt load\n";

    // if we are running into a load (%0 = load %a), we do not need to compute pts(%0),
    // instead, we use pts(%0) to represent alias(%0), and when later pts(%0) is required 
    // at another inst (e.g., %1 = load %0), we use pts(alias(%0)) instead. 
    // which is achievable by
    /*
        alias = getpts(%0)
        for(auto a : alias){
            alias(%1) U= pts(a)
        }
    
    */


    double start = stat->getClk();
    bool changed = false;

    NodeID dstVar = load->getPAGDstNodeID();

    

    // const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());

    // p = *q, the type of p must be a pointer
    if(load->getPAGDstNode()->isPointer()){
        // todo: cannot differentiate alloca variables with others.
        if(load->getPAGSrcNode()->getNodeKind() == SVFValue::ValNodeAlloca){
            outs() << *load << " " << load->getPAGSrcNode()->getNodeKind() << " 1\n";
            if(unionPts(dstVar, load->getPAGSrcNodeID())){
                changed = true;
            }
        }
        else{
            outs() << *load << " " << load->getPAGSrcNode()->getNodeKind() << " 0\n";

            auto alias = getPts(load->getPAGSrcNodeID());
            for(auto pte : alias){
                if(unionPtsFromIn(load, pte, dstVar)){
                    changed = true;
                }
            }

        }

        

        outs() << "New alias for " << dstVar << "\n";
        for(auto pte : getPts(dstVar)){
            outs() << pte << "\n";
        }


        // for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt)
        // {
        //     NodeID ptd = *ptdIt;

        //     if (pag->isConstantObj(ptd))
        //         continue;

        //     if (unionPtsFromIn(load, ptd, dstVar))
        //         changed = true;

        //     if (isFieldInsensitive(ptd))
        //     {
        //         /// If the ptd is a field-insensitive node, we should also get all field nodes'
        //         /// points-to sets and pass them to pagDst.
        //         const NodeBS& allFields = getAllFieldsObjVars(ptd);
        //         for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
        //                 fieldIt != fieldEit; ++fieldIt)
        //         {
        //             if (unionPtsFromIn(load, *fieldIt, dstVar))
        //                 changed = true;
        //         }
        //     }
        // }
    }
    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}