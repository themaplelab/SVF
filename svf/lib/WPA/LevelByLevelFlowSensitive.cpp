

#include "WPA/LevelByLevelFlowSensitive.h"
#include "WPA/WPAStat.h"

// This cannot goes into header. Causing recursive include.
#include "WPA/Steensgaard.h"


using namespace SVF;
using namespace SVFUtil;


size_t LevelByLevelFlowSensitive::computePointerLevels(std::map<NodeID, std::set<NodeID>> &dag){
    size_t maxPl = 0;
    for(auto it = dag.begin(); it != dag.end(); ++it){
        auto pl = getPointerLevel(it->first, dag);
        maxPl = std::max(maxPl, pl);
        pointerLevelToNodeIDsMap[pl].insert(it->first);
    }

    return maxPl;
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


    // JH todo: use all pointers instead of only dag
    size_t highestPointerLevel = computePointerLevels(dag);


    currentPointerLevel = highestPointerLevel;
    svfg = memSSA.buildPTROnlySvfgForPointerLevel(ander, currentPointerLevel, pointerLevelMap);
    setGraph(svfg);

    svfg->dump("maxpl", true);

    dumpTopLevelPtsTo();

    outs() << "End of initialization\n";
}



void LevelByLevelFlowSensitive::analyze(){

    outs() << "Levpa analyze.\n";

    initialize();

    solveConstraints();
    finalize();

    std::terminate();



    // if(Options::WriteAnder().empty()){
    //     // initialize - split pointers into groups

    //     // while not final layer
    //     // build mssa and svfg for current layer

    //     // solve fixed point for current layer

    //     // cleanup


    // }
}


void LevelByLevelFlowSensitive::solveConstraints()
{
    // bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::FsTimeLimit());

    // double start = stat->getClk(true);
    // /// Start solving constraints
    // DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Start Solving Constraints\n"));


    while(currentPointerLevel){
        outs() << "Current pointer level " << currentPointerLevel << "\n";
        do
        {
            numOfIteration++;

            if(0 == numOfIteration % OnTheFlyIterBudgetForStat)
                dumpStat();

            callGraphSCC->find();
            outs() << "1111\n";
            initWorklist();
            outs() << "2222\n";
            solveWorklist();
            outs() << "3333\n";

        }
        while (updateCallGraph(getIndirectCallsites()));
        
        dumpAllPts();

        --currentPointerLevel;
        outs() << "4444\n";
        // this should be updating svfg instead of creating new svfg
        
        svfg = memSSA.buildPTROnlySvfgForPointerLevel(ander, currentPointerLevel, pointerLevelMap);
        setGraph(svfg);

        
        svfg->dump("svfg-pl" + std::to_string(currentPointerLevel), true);
        outs() << "5555\n";


    }

    

    // DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Finish Solving Constraints\n"));

    // // Reset the time-up alarm; analysis is done.
    // SVFUtil::stopAnalysisLimitTimer(limitTimerSet);

    // double end = stat->getClk(true);
    // solveTime += (end - start) / TIMEINTERVAL;

}

bool LevelByLevelFlowSensitive::updateCallGraph(const CallSiteToFunPtrMap& callsites){
    return false;
}


/*!
 * Finalize analysis
 */
void LevelByLevelFlowSensitive::finalize()
{

    outs() << "6666\n";

    getPAG()->dump("pag");
    if(Options::DumpVFG())
        svfg->dump("fs_solved", true);

    outs() << "7777\n";
    
    // JH todo: this cause bug. skip for now.
    // NodeStack& nodeStack = WPASolver<SVFG*>::SCCDetect();

    outs() << "8888\n";

    // while (nodeStack.empty() == false)
    // {
    //     // NodeID rep = nodeStack.top();
    //     nodeStack.pop();
    //     // const NodeBS& subNodes = getSCCDetector()->subNodes(rep);
    //     // if (subNodes.count() > maxSCCSize)
    //     //     maxSCCSize = subNodes.count();
    //     // if (subNodes.count() > 1)
    //     // {
    //     //     numOfNodesInSCC += subNodes.count();
    //     //     numOfSCC++;
    //     // }
    // }
    outs() << "9999\n";

    // TODO: check -stat too.
    if (Options::ClusterFs())
    {
        Map<std::string, std::string> stats;
        const PTDataTy *ptd = getPTDataTy();
        // TODO: should we use liveOnly?
        Map<PointsTo, unsigned> allPts = ptd->getAllPts(true);
        // TODO: parameterise final arg.
        NodeIDAllocator::Clusterer::evaluate(*PointsTo::getCurrentBestNodeMapping(), allPts, stats, true);
        NodeIDAllocator::Clusterer::printStats("post-main: best", stats);

        // Do the same for the candidates. TODO: probably temporary for eval. purposes.
        // for (std::pair<hclust_fast_methods, std::vector<NodeID>> &candidate : candidateMappings)
        // {
        //     // Can reuse stats, since we're always filling it with `evaluate`, it will always be overwritten.
        //     NodeIDAllocator::Clusterer::evaluate(candidate.second, allPts, stats, true);
        //     NodeIDAllocator::Clusterer::printStats("post-main: candidate " + SVFUtil::hclustMethodToString(candidate.first), stats);
        // }
    }
    outs() << "1010101010\n";

    BVDataPTAImpl::finalize();
}


void LevelByLevelFlowSensitive::processNode(NodeID nodeId)
{
    // outs() << "process node: " << nodeId << "\n";
    SVFGNode* node = svfg->getSVFGNode(nodeId);
    
    // outs() << *node << "\n";
    // outs() << "2.2\n";

    if (processSVFGNode(node))
        propagate(&node);

    clearAllDFOutVarFlag(node);
}

bool LevelByLevelFlowSensitive::processSVFGNode(SVFGNode* node)
{
    outs() << "Processing node " << *node << "\n";
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

    outs() << "Processed node " << *node << " " << changed << "\n";

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
        for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt)
        {
            NodeID ptd = *ptdIt;

            if (pag->isConstantObj(ptd))
                continue;

            if (unionPtsFromIn(load, ptd, dstVar))
                changed = true;

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
    }
    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool LevelByLevelFlowSensitive::processStore(const StoreSVFGNode* store)
{

    const PointsTo & dstPts = getPts(store->getPAGDstNodeID());

    outs() << "store dst id: " << store->getPAGDstNodeID() << "\n";

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


    outs() << "Is strong update? " << isSU << " " << changed << "\n";

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
        if(const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(inst)){
            if(pointerLevelMap.at(inst->getDstID()) == currentPointerLevel){
                return true;
            }
        }
        else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(inst)){
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

bool LevelByLevelFlowSensitive::propVarPtsFromSrcToDst(NodeID var, const SVFGNode* src, const SVFGNode* dst)
{
    bool changed = false;
    if (SVFUtil::isa<StoreSVFGNode>(src))
    {
        if (updateInFromOut(src, var, dst, var))
            changed = true;
    }
    else
    {
        if (updateInFromIn(src, var, dst, var))
            changed = true;
    }
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
            outs() << "Looking at child " << *(svfg->getSVFGNode(Node_Index(*EI))) << "\n";
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
    else if(const auto *null = SVFUtil::dyn_cast<NullPtrVFGNode>(node)){
        return 0;
    }

    outs() << *node << "\n";
    assert(false && "Unknown node type.");
    return -1;
}

            



size_t LevelByLevelFlowSensitive::getPointerLevel(NodeID nId){
    auto node = svfg->getSVFGNode(nId);
    return getPointerLevel(node);
}


size_t LevelByLevelFlowSensitive::getPointerLevelFromPagNodeId(NodeID nId){
    if(pointerLevelMap.count(nId)){
        return pointerLevelMap.at(nId);
    }
    return 0;
}
