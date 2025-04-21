//===- SVFGBuilder.cpp -- SVFG builder----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFGBuilder.cpp
 *
 *  Created on: Apr 15, 2014
 *      Author: Yulei Sui
 */
#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "MSSA/MemSSA.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"
#include "Graphs/CallGraph.h"


using namespace SVF;
using namespace SVFUtil;





// JH todo: place holder, to be updated.
void SVFGBuilder::updateMssaForSvfg(std::unique_ptr<SVF::MemSSA> &mssa){
    svfg = std::unique_ptr<SVFG>(new SVFG(std::move(mssa), VFG::PTRONLYSVFG));
}

void SVFGBuilder::updateSVFGForPointerLevel(size_t pl, std::map<NodeID, size_t>& plMap){

    svfg->updateSVFGForPointerLevel(pl, plMap);

}

void SVFGBuilder::updateMssaForPointerLevel(BVDataPTAImpl* pta, bool ptrOnlyMSSA, size_t pl, std::map<NodeID, size_t>& plMap, MemSSA *mssa){

    DBOUT(DGENERAL, outs() << pasMsg("Update Memory SSA \n"));

    mssa->updateDetail(pta, ptrOnlyMSSA);

    CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {

        const FunObjVar *fun = item.second->getFunction();
        if (isExtCall(fun))
            continue;

        mssa->buildMemSsaForPointerLevel(*fun, pl, plMap);
    }
    // outs() << "MSSA for pointer level " << pl << "\n";
    // mssa->dumpMSSA();


    mssa->performStat();
    if (Options::DumpMSSA())
    {
        mssa->dumpMSSA();
    }

    return;
}



void SVFGBuilder::updatePTROnlySvfgForPointerLevel(BVDataPTAImpl* pta, size_t pl, std::map<NodeID, size_t>& plMap){

    // JH todo: should also update rather than give new.
    updateMssaForPointerLevel(pta, true, pl, plMap, svfg->getMSSA());
    // svfg->getMSSA();

    // updateMssaForSvfg(mssa);

    // updateSVFGForPointerLevel(pl, plMap);
    buildSVFGForPointerLevel(pl, plMap);

    /// Update call graph using pre-analysis results
    if(Options::SVFGWithIndirectCall() || SVFGWithIndCall)
        svfg->updateCallGraph(pta);

    if(svfg->getMSSA()->getPTA()->printStat())
        svfg->performStat();

    if(Options::DumpVFG())
        svfg->dump("svfg_final");

}

SVFG* SVFGBuilder::buildPTROnlySvfgForPointerLevel(BVDataPTAImpl* pta, size_t pl, std::map<NodeID, size_t>& plMap){
    return buildPointerLevel(pta, VFG::PTRONLYSVFG, pl, plMap);
}

SVFG* SVFGBuilder::buildPointerLevel(BVDataPTAImpl* pta, VFG::VFGK kind, size_t pl, std::map<NodeID, size_t>& plMap){

    outs() << "2.1\n";
    

    // JH todo: build mssa for each layer.
    auto mssa = buildMssaForPointerLevel(pta, (VFG::PTRONLYSVFG==kind || VFG::PTRONLYSVFG_OPT==kind), pl, plMap);
    // mssa->dumpMSSA();

    DBOUT(DGENERAL, outs() << pasMsg("Build Sparse Value-Flow Graph \n"));
    if(kind == VFG::FULLSVFG_OPT || kind == VFG::PTRONLYSVFG_OPT)
        svfg = std::make_unique<SVFGOPT>(std::move(mssa), kind);
    else
        svfg = std::unique_ptr<SVFG>(new SVFG(std::move(mssa),kind));

    outs() << "2.2\n";
    
    buildSVFGForPointerLevel(pl, plMap);

    outs() << "2.3\n";


    /// Update call graph using pre-analysis results
    if(Options::SVFGWithIndirectCall() || SVFGWithIndCall)
        svfg->updateCallGraph(pta);

    if(svfg->getMSSA()->getPTA()->printStat())
        svfg->performStat();

    if(Options::DumpVFG())
        svfg->dump("svfg_final");

    return svfg.get();
}


std::unique_ptr<MemSSA> SVFGBuilder::buildMssaForPointerLevel(BVDataPTAImpl* pta, bool ptrOnlyMSSA, size_t pl, std::map<NodeID, size_t>& plMap){

    DBOUT(DGENERAL, outs() << pasMsg("Build Memory SSA \n"));

    // Do not need to update, it is just preparation.
    auto mssa = std::make_unique<MemSSA>(pta, ptrOnlyMSSA);



    CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {

        const FunObjVar *fun = item.second->getFunction();
        if (isExtCall(fun))
            continue;

        mssa->buildMemSsaForPointerLevel(*fun, pl, plMap);
    }
    mssa->dumpMSSA();


    mssa->performStat();
    if (Options::DumpMSSA())
    {
        mssa->dumpMSSA();
    }

    return mssa;
}






SVFG* SVFGBuilder::buildPTROnlySVFG(BVDataPTAImpl* pta)
{
    if(Options::OPTSVFG())
        return build(pta, VFG::PTRONLYSVFG_OPT);
    else
        return build(pta, VFG::PTRONLYSVFG);
}

SVFG* SVFGBuilder::buildFullSVFG(BVDataPTAImpl* pta)
{
    return build(pta, VFG::FULLSVFG);
}


void SVFGBuilder::buildSVFGForPointerLevel(size_t pl, std::map<NodeID, size_t>& plMap){
    svfg->buildSVFGForPointerLevel(pl, plMap);
}

/*!
 * Create SVFG
 */
void SVFGBuilder::buildSVFG()
{
    svfg->buildSVFG();
}

/// Create DDA SVFG
SVFG* SVFGBuilder::build(BVDataPTAImpl* pta, VFG::VFGK kind)
{
    // JH todo: build mssa for each layer.
    auto mssa = buildMSSA(pta, (VFG::PTRONLYSVFG==kind || VFG::PTRONLYSVFG_OPT==kind));

    DBOUT(DGENERAL, outs() << pasMsg("Build Sparse Value-Flow Graph \n"));
    if(kind == VFG::FULLSVFG_OPT || kind == VFG::PTRONLYSVFG_OPT)
        svfg = std::make_unique<SVFGOPT>(std::move(mssa), kind);
    else
        svfg = std::unique_ptr<SVFG>(new SVFG(std::move(mssa),kind));
    buildSVFG();

    /// Update call graph using pre-analysis results
    if(Options::SVFGWithIndirectCall() || SVFGWithIndCall)
        svfg->updateCallGraph(pta);

    if(svfg->getMSSA()->getPTA()->printStat())
        svfg->performStat();

    if(Options::DumpVFG())
        svfg->dump("svfg_final");

    return svfg.get();
}

/*!
 * Release memory
 */
void SVFGBuilder::releaseMemory()
{
    svfg->clearMSSA();
}

std::unique_ptr<MemSSA> SVFGBuilder::buildMSSA(BVDataPTAImpl* pta, bool ptrOnlyMSSA)
{

    DBOUT(DGENERAL, outs() << pasMsg("Build Memory SSA \n"));

    auto mssa = std::make_unique<MemSSA>(pta, ptrOnlyMSSA);

    CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {

        const FunObjVar *fun = item.second->getFunction();
        if (isExtCall(fun))
            continue;

        mssa->buildMemSSA(*fun);
    }

    mssa->performStat();
    if (Options::DumpMSSA())
    {
        mssa->dumpMSSA();
    }

    return mssa;
}


