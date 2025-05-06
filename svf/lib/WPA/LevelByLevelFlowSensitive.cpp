

#include "WPA/LevelByLevelFlowSensitive.h"
#include "WPA/WPAStat.h"

// This cannot goes into header. Causing recursive include.
#include "WPA/Steensgaard.h"


using namespace SVF;
using namespace SVFUtil;


size_t LevelByLevelFlowSensitive::computeMaxPointerLevel(std::map<NodeID, std::set<NodeID>> &dag){

    size_t maxPl = 0;
    auto preAnalysis = getPreAnalysis();
    for(auto it = preAnalysis->getAllValidPtrs().begin(); it != preAnalysis->getAllValidPtrs().end(); ++it){
        SVF::NodeID nid = *it;
        auto pl = computePointerLevel(nid, dag);
        maxPl = std::max(maxPl, pl);
        pointerLevelToNodeIDsMap[pl].insert(nid);
    }

    return maxPl;
}


size_t LevelByLevelFlowSensitive::computePointerLevel(NodeID id, std::map<NodeID, std::set<NodeID>> &dag){

    auto repNode = ptgScc->repNode(id);
    
    if(!dag.count(repNode)){
        pointerLevelMap[id] = 0;
        return 0;
    }
    
    if(pointerLevelMap.count(repNode)){
        pointerLevelMap[id] = pointerLevelMap.at(repNode);
        return pointerLevelMap.at(repNode);
    }
    
    size_t pl = 0;
    for(auto to : dag.at(repNode)){
        pl = std::max(pl, computePointerLevel(to, dag));
    }

    // pointerLevelMap[id] = pl+1;
    pointerLevelMap[repNode] = pl+1;
    pointerLevelMap[id] = pointerLevelMap.at(repNode);
    return pointerLevelMap[repNode];

}

void LevelByLevelFlowSensitive::initialize(){
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
}

void LevelByLevelFlowSensitive::analyze(){

    initialize();
    solveConstraints();
    finalize();
}

void LevelByLevelFlowSensitive::solveConstraints(){

    bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::FsTimeLimit());
    double start = stat->getClk(true);

    while(true){
        outs() << "Solving pointer level " << currentPointerLevel << "\n";
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
        // JH todo: this should be updating svfg instead of creating new svfg
        memSSA.updatePTROnlySvfgForPointerLevel(this, currentPointerLevel, pointerLevelMap);
        svfg->dump("svfg-pl" + std::to_string(currentPointerLevel), true);
    }

    SVFUtil::stopAnalysisLimitTimer(limitTimerSet);
    double end = stat->getClk(true);
    solveTime += (end - start) / TIMEINTERVAL;
}

bool LevelByLevelFlowSensitive::updateCallGraph(const CallSiteToFunPtrMap& callsites){
    // JH todo: update according to fspta
    return false;
}

/*!
 * Finalize analysis
 */
void LevelByLevelFlowSensitive::finalize(){

    if(Options::DumpVFG())
        svfg->dump("fs_solved", true);

    BVDataPTAImpl::finalize();
}

void LevelByLevelFlowSensitive::processNode(NodeID nodeId){

    SVFGNode* node = svfg->getSVFGNode(nodeId);

    if (processSVFGNode(node))
        propagate(&node);

    clearAllDFOutVarFlag(node);
}

bool LevelByLevelFlowSensitive::processSVFGNode(SVFGNode* node){

    outs() << "Processing svfg node " << *node << "\n";
    double start = stat->getClk();
    bool changed = false;
    if (AddrSVFGNode* addr = SVFUtil::dyn_cast<AddrSVFGNode>(node))
    {
        numOfProcessedAddr++;
        if (processAddr(addr))
            changed = true;
    }
    else if (CopySVFGNode* copy = SVFUtil::dyn_cast<CopySVFGNode>(node))
    {
        numOfProcessedCopy++;
        if (processCopy(copy))
            changed = true;
    }
    else if (GepSVFGNode* gep = SVFUtil::dyn_cast<GepSVFGNode>(node))
    {
        numOfProcessedGep++;
        if(processGep(gep))
            changed = true;
    }
    else if (LoadSVFGNode* load = SVFUtil::dyn_cast<LoadSVFGNode>(node))
    {
        numOfProcessedLoad++;
        if(processLoad(load))
            changed = true;
    }
    else if (StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        numOfProcessedStore++;
        if (processStore(store))
            changed = true;
    }
    else if (PHISVFGNode* phi = SVFUtil::dyn_cast<PHISVFGNode>(node))
    {
        numOfProcessedPhi++;
        if (processPhi(phi))
            changed = true;
    }
    else if (SVFUtil::isa<MSSAPHISVFGNode, FormalINSVFGNode,
             FormalOUTSVFGNode, ActualINSVFGNode,
             ActualOUTSVFGNode>(node))
    {
        numOfProcessedMSSANode++;
        changed = true;
    }
    else if (SVFUtil::isa<ActualParmSVFGNode, FormalParmSVFGNode,
             ActualRetSVFGNode, FormalRetSVFGNode,
             NullPtrSVFGNode>(node))
    {
        changed = true;
    }
    else if (SVFUtil::isa<CmpVFGNode, BinaryOPVFGNode>(node) ||
             SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
    {
    }
    else
    {
        assert(false && "unexpected kind of SVFG nodes");
    }

    double end = stat->getClk();
    processTime += (end - start) / TIMEINTERVAL;

    return changed;
}

bool LevelByLevelFlowSensitive::processAddr(const AddrSVFGNode* addr)
{
    double start = stat->getClk();
    NodeID srcID = addr->getPAGSrcNodeID();
    /// TODO: If this object has been set as field-insensitive, just
    ///       add the insensitive object node into dst pointer's pts.
    if (isFieldInsensitive(srcID))
        srcID = getFIObjVar(srcID);
    bool changed = addPts(addr->getPAGDstNodeID(), srcID);
    double end = stat->getClk();
    addrTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processCopy(const CopySVFGNode* copy)
{
    double start = stat->getClk();
    bool changed = unionPts(copy->getPAGDstNodeID(), copy->getPAGSrcNodeID());
    double end = stat->getClk();
    copyTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processPhi(const PHISVFGNode* phi)
{
    double start = stat->getClk();
    bool changed = false;
    NodeID pagDst = phi->getRes()->getId();
    for (PHISVFGNode::OPVers::const_iterator it = phi->opVerBegin(), eit = phi->opVerEnd();	it != eit; ++it)
    {
        NodeID src = it->second->getId();
        const PointsTo& srcPts = getPts(src);
        if (unionPts(pagDst, srcPts))
            changed = true;
    }

    double end = stat->getClk();
    phiTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processGep(const GepSVFGNode* edge)
{
    double start = stat->getClk();
    bool changed = false;
    const PointsTo& srcPts = getPts(edge->getPAGSrcNodeID());

    PointsTo tmpDstPts;
    const GepStmt* gepStmt = SVFUtil::cast<GepStmt>(edge->getPAGEdge());
    if (gepStmt->isVariantFieldGep())
    {
        for (NodeID o : srcPts)
        {
            if (isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            setObjFieldInsensitive(o);
            tmpDstPts.set(getFIObjVar(o));
        }
    }
    else
    {
        for (NodeID o : srcPts)
        {
            if (isBlkObjOrConstantObj(o) || isFieldInsensitive(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            NodeID fieldSrcPtdNode = getGepObjVar(o, gepStmt->getAccessPath().getConstantStructFldIdx());
            tmpDstPts.set(fieldSrcPtdNode);
        }
    }

    if (unionPts(edge->getPAGDstNodeID(), tmpDstPts))
        changed = true;

    double end = stat->getClk();
    gepTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();
    bool changed = false;

    NodeID dstVar = load->getPAGDstNodeID();

    const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());

    // p = *q, the type of p must be a pointer
    if(load->getPAGDstNode()->isPointer())
    {
        outs() << "load " << load->getId() << " " << *load << "\n";
        for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt)
        {
            NodeID ptd = *ptdIt;
            outs() << "pts1 " << ptd << "\n";
            if (pag->isConstantObj(ptd))
                continue;

            for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), ptd)){
                outs() << "before " << pte << "\n";
            }

            if (unionPtsFromIn(load, ptd, dstVar))
                changed = true;

            for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), ptd)){
                outs() << "after " << pte << "\n";
            }

            if (isFieldInsensitive(ptd))
            {
                /// If the ptd is a field-insensitive node, we should also get all field nodes'
                /// points-to sets and pass them to pagDst.
                const NodeBS& allFields = getAllFieldsObjVars(ptd);
                for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                        fieldIt != fieldEit; ++fieldIt)
                {
                    if (unionPtsFromIn(load, *fieldIt, dstVar))
                        changed = true;
                }
            }
        }

        for(auto p : getPts(dstVar)){
            outs() << "result " << p << "\n";
        }

    }
    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processStore(const StoreSVFGNode* store)
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
        for (PointsTo::iterator it = dstPts.begin(), eit = dstPts.end(); it != eit; ++it)
        {
            NodeID ptd = *it;

            if (pag->isConstantObj(ptd))
                continue;

            if (unionPtsFromTop(store, store->getPAGSrcNodeID(), ptd))
                changed = true;
        }
    }

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

bool LevelByLevelFlowSensitive::isStrongUpdate(const SVFGNode* node, NodeID& singleton)
{
    bool isSU = false;
    if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        const PointsTo& dstCPSet = getPts(store->getPAGDstNodeID());
        if (dstCPSet.count() == 1)
        {
            /// Find the unique element in cpts
            PointsTo::iterator it = dstCPSet.begin();
            singleton = *it;

            // Strong update can be made if this points-to target is not heap, array or field-insensitive.
            if (!isHeapMemObj(singleton) && !isArrayMemObj(singleton))
            {
                assert(pag->getBaseObject(singleton)->isFieldInsensitive() == pag->getBaseObject(singleton)->isFieldInsensitive());
                if (pag->getBaseObject(singleton)->isFieldInsensitive() == false
                        && !isLocalVarInRecursiveFun(singleton))
                {
                    isSU = true;
                }
            }
        }
    }
    return isSU;
}

bool LevelByLevelFlowSensitive::hasCurrentPointerLevel(NodeID nId){
    auto svfgNode = svfg->getSVFGNode(nId);
    auto icfgNode = svfgNode->getICFGNode();
    auto stmts = icfgNode->getSVFStmts();

    
    for(auto inst : stmts){
        if(SVFUtil::isa<LoadStmt>(inst)){
            if(pointerLevelMap.at(inst->getDstID()) == currentPointerLevel){
                return true;
            }
        }
        else if (SVFUtil::isa<StoreStmt>(inst)){
            if(pointerLevelMap.at(inst->getDstID()) == currentPointerLevel){
                return true;
            }
        }

    }
    return false;
}

bool LevelByLevelFlowSensitive::propFromSrcToDst(SVFGEdge* edge)
{
    double start = stat->getClk();
    bool changed = false;

    if (DirectSVFGEdge* dirEdge = SVFUtil::dyn_cast<DirectSVFGEdge>(edge))
        changed = propAlongDirectEdge(dirEdge);
    else if (IndirectSVFGEdge* indEdge = SVFUtil::dyn_cast<IndirectSVFGEdge>(edge))
        changed = propAlongIndirectEdge(indEdge);
    // else if (ByPassingSVFGEdge *bpEdge = SVFUtil::dyn_cast<ByPassingSVFGEdge>(edge)){
    //     changed = propAlongIndirectEdge(indEdge);

    // }
    else
        assert(false && "new kind of svfg edge?");

    double end = stat->getClk();
    propagationTime += (end - start) /TIMEINTERVAL;
    return changed;
}

/*!
 * Propagate points-to information along DIRECT SVFG edge.
 */
bool LevelByLevelFlowSensitive::propAlongDirectEdge(const DirectSVFGEdge* edge)
{
    double start = stat->getClk();
    bool changed = false;

    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();
    // If this is an actual-param or formal-ret, top-level pointer's pts must be
    // propagated from src to dst.
    if (ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(src))
        changed = propagateFromAPToFP(ap, dst);
    else if (FormalRetSVFGNode* fp = SVFUtil::dyn_cast<FormalRetSVFGNode>(src))
        changed = propagateFromFRToAR(fp, dst);
    else if(ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(dst)){
        changed = propagateToAR(src, ap);
    }
    else if(FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(src)){
        changed = propagateFromFP(fp, dst);
    }
    else
    {
        // Direct SVFG edge links between def and use of a top-level pointer.
        // There's no points-to information propagated along direct edge.
        // Since the top-level pointer's value has been changed at src node,
        // return TRUE to put dst node into the work list.




        changed = true;
    }

    double end = stat->getClk();
    directPropaTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::propagateFromAPToFP(const ActualParmSVFGNode* ap, const SVFGNode* dst)
{
    const FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(dst);
    assert(fp && "expecting a formal param node");

    NodeID pagDst = fp->getParam()->getId();
    const PointsTo &srcCPts = getPts(ap->getParam()->getId());
    bool changed = unionPts(pagDst, srcCPts);

    // bool changed = false;

    for(auto alias : srcCPts){
        outs() << "alias at AP: " << alias << "\n";
        // if(unionPtsFromIn(ap, alias, pagDst)){
        //     changed = true;
        // }
        for(auto p : getDFInPtsSet(ap, alias)){
            outs() << "in: " << p << "\n";
        }

        if(updateOutFromIn(ap, alias, fp, alias)){
            changed = true;
        }
        // if(updateInFromIn(fp, alias, ap, alias)){
        //     changed = true;
        // }
    }

    



    // for(auto pte : getPts(ap->getParam()->getId())){
    //     if(updateInFromIn(ap, pte, dst, pte)){
    //         updateOutFromIn(dst, pte, dst, pte);
    //         changed = true;
    //     }
    // }

    outs() << "ACTUAL TO FORMAL changed? " << changed << "\n"; 

    return changed;
}

bool LevelByLevelFlowSensitive::propagateFromFRToAR(const FormalRetSVFGNode* fr, const SVFGNode* dst)
{
    const ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(dst);
    assert(ar && "expecting an actual return node");

    NodeID pagDst = ar->getRev()->getId();
    const PointsTo & srcCPts = getPts(fr->getRet()->getId());
    bool changed = unionPts(pagDst, srcCPts);

    return changed;
}

bool LevelByLevelFlowSensitive::propagateToAR(const SVFGNode* src, const SVFGNode* dst){
    
    outs() << "Propagate to AR " << src->getId() << " => " << dst->getId() << "\n";
    
    const LoadSVFGNode* ar = SVFUtil::dyn_cast<LoadSVFGNode>(src);
    assert(ar && "expecting an actual return node");

    const ActualParmSVFGNode *ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(dst);
    assert(ap && "excepting an actual param as dst");


    if(const LoadSVFGNode *load = SVFUtil::dyn_cast<LoadSVFGNode>(src)){
        auto alias = getPts(load->getPAGSrcNodeID());
        
        for(auto a : alias){
            outs() << "alias is " << a << "\n";
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), a)){
                outs() << "load before " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(dst->getId(), a)){
                outs() << "dst before " << pte << "\n";
            }

            // auto test = propVarPtsFromSrcToDst(a, load, dst);
            // JH: no idea why updateInFromIn not working while updateInFromOut is working.
            auto test = updateInFromIn(load, a, dst, a);
            outs() << test << "\n";
            test = updateInFromOut(load, a, dst, a);
            outs() << test << "\n";

            // for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), a)){
            //     if(updateInFromIn(load, pte, dst, pte)){
            //         // updateOutFromIn(load, pte, )
            //     }
            // }
            for(auto pte : getDFPTDataTy()->getDFOutPtsSet(load->getId(), a)){
                outs() << "load after " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(dst->getId(), a)){
                outs() << "dst in after " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFOutPtsSet(dst->getId(), a)){
                outs() << "dst out after " << pte << "\n";
            }
            
            
        }
    }
    return true;
}

bool LevelByLevelFlowSensitive::propagateFromFP(const SVFGNode* src, const SVFGNode* dst){
    
    outs() << "Propagate from FP " << src->getId() << " => " << dst->getId() << "\n";
    

    const FormalParmSVFGNode *fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(src);
    assert(fp && "excepting an formal param as src");


    if(const StoreSVFGNode *store = SVFUtil::dyn_cast<StoreSVFGNode>(dst)){
        auto alias = getPts(fp->getParam()->getId());
        
        for(auto a : alias){
            outs() << "alias is " << a << "\n";
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(fp->getId(), a)){
                outs() << "fp before " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(store->getId(), a)){
                outs() << "dst before " << pte << "\n";
            }

            // auto test = propVarPtsFromSrcToDst(a, load, dst);
            // JH: no idea why updateInFromIn not working while updateInFromOut is working.
            auto test = updateInFromIn(fp, a, store, a);
            outs() << test << "\n";
            test = updateInFromOut(fp, a, store, a);
            outs() << test << "\n";

            // for(auto pte : getDFPTDataTy()->getDFInPtsSet(load->getId(), a)){
            //     if(updateInFromIn(load, pte, dst, pte)){
            //         // updateOutFromIn(load, pte, )
            //     }
            // }
            for(auto pte : getDFPTDataTy()->getDFOutPtsSet(fp->getId(), a)){
                outs() << "fp after " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFInPtsSet(store->getId(), a)){
                outs() << "dst in after " << pte << "\n";
            }
            for(auto pte : getDFPTDataTy()->getDFOutPtsSet(store->getId(), a)){
                outs() << "dst out after " << pte << "\n";
            }
            
            
        }
    }
    return true;
}


bool LevelByLevelFlowSensitive::propVarPtsFromSrcToDst(NodeID var, const SVFGNode* src, const SVFGNode* dst)
{
    bool changed = false;
    if (SVFUtil::isa<StoreSVFGNode>(src))
    {
        outs() << "before update: \n" << var << " => \n";
        for(auto p : getDFOutPtsSet(src, var)){
            outs() << p << "\n";
        }
        if (updateInFromOut(src, var, dst, var))
            changed = true;

        outs() << "after update: \n" << var << " => \n";
            for(auto p : getDFInPtsSet(dst, var)){
                outs() << p << "\n";
            }
    }
    else
    {
        if (updateInFromIn(src, var, dst, var))
            changed = true;
    }

    // if(!SVFUtil::isa<StoreSVFGNode>(dst)){
    //     if(updateOutFromIn(dst, var, dst, var)){
    //         changed = true;
    //     }
    // }

    outs() << "Changed? " << changed << "\n";
    return changed;
}


bool LevelByLevelFlowSensitive::propAlongIndirectEdge(const IndirectSVFGEdge* edge)
{
    double start = stat->getClk();

    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();

    bool changed = false;

    // Get points-to targets may be used by next SVFG node.
    // Propagate points-to set for node used in dst.
    const NodeBS& pts = edge->getPointsTo();
    for (NodeBS::iterator ptdIt = pts.begin(), ptdEit = pts.end(); ptdIt != ptdEit; ++ptdIt)
    {
        NodeID ptd = *ptdIt;
        outs() << "pass " << ptd << " from " << src->getId() << " to " << dst->getId() << "\n";
        if (propVarPtsFromSrcToDst(ptd, src, dst))
            changed = true;

        if (isFieldInsensitive(ptd))
        {
            /// If this is a field-insensitive obj, propagate all field node's pts
            const NodeBS& allFields = getAllFieldsObjVars(ptd);
            for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                    fieldIt != fieldEit; ++fieldIt)
            {
                if (propVarPtsFromSrcToDst(*fieldIt, src, dst))
                    changed = true;
            }
        }
    }

    double end = stat->getClk();
    indirectPropaTime += (end - start) / TIMEINTERVAL;
    return changed;
}


size_t LevelByLevelFlowSensitive::getPointerLevel(SVFGNode *node){
    if(const auto *store = SVFUtil::dyn_cast<StoreSVFGNode>(node)){
        return getPointerLevelFromPagNodeId(store->getPAGSrcNodeID());
    }
    else if(const auto *stmt = SVFUtil::dyn_cast<StmtVFGNode>(node)){
        return getPointerLevelFromPagNodeId(stmt->getPAGDstNodeID());
    }
    else if(const auto *argu = SVFUtil::dyn_cast<ArgumentVFGNode>(node)){
        return getPointerLevelFromPagNodeId(argu->getParam()->getId());
    }
    else if(const auto *mr = SVFUtil::dyn_cast<MRSVFGNode>(node)){
        // check svfg children
        child_iterator EI = GTraits::direct_child_begin(mr);
        child_iterator EE = GTraits::direct_child_end(mr);
        for (; EI != EE; ++EI){
            if(SVFUtil::isa<MRSVFGNode>(svfg->getSVFGNode(Node_Index(*EI)))){
                continue;
            }
            return getPointerLevel(svfg->getSVFGNode(Node_Index(*EI)));
        }

        return 0;
        
    }
    else if(const auto *phi = SVFUtil::dyn_cast<PHISVFGNode>(node)){
        return getPointerLevelFromPagNodeId(phi->getRes()->getId());

    }
    else if(SVFUtil::isa<NullPtrVFGNode>(node)){
        return 0;
    }

    assert(false && "Unknown node type.");
    return -1;
}

size_t LevelByLevelFlowSensitive::getPointerLevel(NodeID nId){
    auto node = svfg->getSVFGNode(nId);
    return getPointerLevel(node);
}

size_t LevelByLevelFlowSensitive::getPointerLevelFromPagNodeId(NodeID nId){
    auto repNode = ptgScc->repNode(nId);
    if(pointerLevelMap.count(repNode)){
        return pointerLevelMap.at(repNode);
    }
    return 0;
}
