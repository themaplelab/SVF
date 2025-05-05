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
        svfg->addByPassingEdges();
        svfg->dump("svfg-pl" + std::to_string(currentPointerLevel), true);
    }

    SVFUtil::stopAnalysisLimitTimer(limitTimerSet);
    double end = stat->getClk(true);
    solveTime += (end - start) / TIMEINTERVAL;

    outs() << "PTS after pointer level 2\n";
    dumpTopLevelPtsTo();
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

    

    const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());

    // p = *q, the type of p must be a pointer
    if(load->getPAGDstNode()->isPointer()){
        // todo: cannot differentiate alloca variables with others.
        // if(false){
        if(load->getPAGSrcNode()->getNodeKind() == SVFValue::ValNodeAlloca){

            outs() << *load << " " << load->getPAGSrcNode()->getNodeKind() << " 1\n";
            if(unionPts(dstVar, load->getPAGSrcNodeID())){
                changed = true;
            }
            // if(unionPtsFromIn(load, load->getPAGSrcNodeID(), dstVar)){
            //     changed = true;
            // }
            for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt){
                NodeID ptd = *ptdIt;
                outs() << "confirmed " << ptd << "\n";
                for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), ptd)){
                    outs() << "before " << pte << "\n";
                }

                if(updateOutFromIn(load, ptd, load, ptd)){
                    changed = true;
                }

                for(auto pte : getDFPTDataTy()->getDFOutPtsSet(load->getId(), ptd)){
                    outs() << "after " << pte << "\n";
                }
            }
        }
        else{
            outs() << *load << " " << load->getPAGSrcNode()->getNodeKind() << " 0\n";

            auto alias = getPts(load->getPAGSrcNodeID());
            for(auto a : alias){
                outs() << "alias " << a << "\n";
                // if(unionPts(dstVar, pte)){
                //     changed = true;
                // }
                // if(unionPtsFromIn(load, a, dstVar)){
                //     changed = true;
                // }


                for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getPAGDstNodeID(), a)){
                    outs() << "pte " << pte << "\n";
                    if(unionPtsFromIn(load, pte, dstVar)){
                        changed = true;
                    }
                }
                
            }

        }

        

        outs() << "New alias for " << dstVar << "\n";
        for(auto pte : getPts(dstVar)){
            outs() << pte << "\n";
        }

        outs() << "Changed? " << changed << "\n";


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


bool LevelByLevelFlowSensitiveOptimized::processStore(const StoreSVFGNode* store)
{

    const PointsTo & dstPts = getPts(store->getPAGDstNodeID());

    /// STORE statement can only be processed if the pointer on the LHS
    /// points to something. If we handle STORE with an empty points-to
    /// set, the OUT set will be updated from IN set. Then if LHS pointer
    /// points-to one target and it has been identified as a strong
    /// update, we can't remove those points-to information computed
    /// before this strong update from the OUT set.
    if (dstPts.empty())
        return false;

    double start = stat->getClk();
    bool changed = false;

    // *p = q, the type of q must be a pointer
    if(getPts(store->getPAGSrcNodeID()).empty() == false && store->getPAGSrcNode()->isPointer())
    {

        if(store->getPAGSrcNode()->getNodeKind() == SVFValue::ValNodeAlloca){

            for (PointsTo::iterator it = dstPts.begin(), eit = dstPts.end(); it != eit; ++it){
                NodeID ptd = *it;

                if (pag->isConstantObj(ptd))
                    continue;

                if (unionPtsFromTop(store, store->getPAGSrcNodeID(), ptd))
                    changed = true;


                // verify result.
                for(auto dp : dstPts){
                    outs() << dp << " => \n";
                    for(auto pte : getDFPTDataTy()->getDFOutPtsSet(store->getId(), dp)){
                        outs() << pte << "\n";
                    }
                }
            }
        }
        else{
            for (PointsTo::iterator it = dstPts.begin(), eit = dstPts.end(); it != eit; ++it)
            {
                NodeID ptd = *it;
                for(auto alias : getPts(store->getPAGSrcNodeID())){
                    outs() << *store << "\n";
                    outs() << "Store alias " << alias << "\n";

                    for(auto p : getDFInPtsSet(store, alias)){
                        outs() << "IN " << p << "\n";
                    }

                    for(auto p : getDFOutPtsSet(store, alias)){
                        outs() << "OUT " << p << "\n";
                    }


                    if (pag->isConstantObj(alias))
                        continue;
                        
                    outs() << getDFPTDataTy()->getDFInPtsSet(store->getId(), alias).count() << "\n";
                
                    if (unionPtsFromIn(store, alias, ptd))
                        changed = true;
                }
            }      
        }
    }

    outs() << "CHANGED? " << changed << "\n";

    double end = stat->getClk();
    storeTime += (end - start) / TIMEINTERVAL;

    double updateStart = stat->getClk();
    // also merge the DFInSet to DFOutSet.
    /// check if this is a strong updates store
    NodeID singleton;
    bool isSU = isStrongUpdate(store, singleton);
    if (isSU)
    {
        svfgHasSU.set(store->getId());
        if (strongUpdateOutFromIn(store, singleton))
            changed = true;
    }
    else
    {
        svfgHasSU.reset(store->getId());
        if (weakUpdateOutFromIn(store))
            changed = true;
    }
    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    return changed;
}
