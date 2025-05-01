

#ifndef LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_
#define LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_

#include "Graphs/SVFGOPT.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAFSSolver.h"
#include "Graphs/PointsToGraph.h"
#include "MSSA/SVFGBuilder.h"
#include <iostream>
// #include "WPA/Andersen.h"





namespace SVF{

    // class AndersenWaveDiff;
    // class SVFModule;

    class Steensgaard;
    class AndersenWaveDiff;
    // class AndersenBase;


    typedef WPAFSSolver<SVFG*> WPASVFGFSSolver;

    class LevelByLevelFlowSensitive : public WPASVFGFSSolver, public BVDataPTAImpl{

        friend class LevelByLevelFlowSensitiveStat;


        public:
            typedef BVDataPTAImpl::MutDFPTDataTy MutDFPTDataTy;
            typedef BVDataPTAImpl::MutDFPTDataTy::DFPtsMap DFInOutMap;
            typedef BVDataPTAImpl::MutDFPTDataTy::PtsMap PtsMap;
            // JH todo : add PointToGraph
            typedef SCCDetection<PointsToGraph*> PointsToGraphSCC;

            // JH todo: update type of pag
            explicit LevelByLevelFlowSensitive(SVFIR* _pag, PTATY type = FSSPARSE_WPA) : WPASVFGFSSolver(), BVDataPTAImpl(_pag, type), ptgScc(nullptr){
                
            }

            void analyze() override;
            void initialize() override;
            virtual void solveConstraints();
            void finalize() override;



            // JH todo: delete ptg and other memory 
            ~LevelByLevelFlowSensitive() override = default;

            inline void pointsToGraphSCCDetection(){
                
                if(ptgScc == nullptr){
                    ptgScc = new PointsToGraphSCC(ptg);
                }
                ptgScc->find();            
            }

            

        protected:

            bool updateCallGraph(const CallSiteToFunPtrMap& callsites) override;

            virtual inline void initWorklist() override{
                if(allNodes.empty()){
                    NodeStack &nodeStack = SCCDetect();
                    while (!nodeStack.empty())
                    {
                        NodeID nodeId = nodeStack.top();
                        nodeStack.pop();
                        allNodes.push_back(nodeId);
                    }
                }
                for(auto nId : allNodes){
                    if(getPointerLevel(nId) == currentPointerLevel){
                        pushIntoWorklist(nId);
                    }
                }
            }
            inline void clearAllDFOutVarFlag(const SVFGNode* stmt)
            {
                getDFPTDataTy()->clearAllDFOutUpdatedVar(stmt->getId());
            }

            virtual inline bool unionPtsFromIn(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar)
            {
                return getDFPTDataTy()->updateTLVPts(stmt->getId(),srcVar,dstVar);
            }
            virtual inline bool unionPtsFromTop(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar)
            {
                return getDFPTDataTy()->updateATVPts(srcVar,stmt->getId(),dstVar);
            }

            virtual bool weakUpdateOutFromIn(const SVFGNode* node)
            {
                return getDFPTDataTy()->updateAllDFOutFromIn(node->getId(),0,false);
            }
            /// Handle strong updates
            virtual bool strongUpdateOutFromIn(const SVFGNode* node, NodeID singleton)
            {
                return getDFPTDataTy()->updateAllDFOutFromIn(node->getId(),singleton,true);
            }
            virtual inline bool updateInFromIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
            {
                return getDFPTDataTy()->updateDFInFromIn(srcStmt->getId(),srcVar, dstStmt->getId(),dstVar);
            }
            virtual inline bool updateInFromOut(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
            {
                return getDFPTDataTy()->updateDFInFromOut(srcStmt->getId(),srcVar, dstStmt->getId(),dstVar);
            }

            void processNode(NodeID nodeId) override;
            bool processSVFGNode(SVFGNode* node);
            bool processAddr(const AddrSVFGNode* addr);
            bool processCopy(const CopySVFGNode* copy);
            bool processPhi(const PHISVFGNode* phi);
            bool processGep(const GepSVFGNode* edge);
            virtual bool processLoad(const LoadSVFGNode* load);
            bool processStore(const StoreSVFGNode* store);

            bool isStrongUpdate(const SVFGNode* node, NodeID& singleton);

            bool hasCurrentPointerLevel(NodeID nId);
            virtual bool propFromSrcToDst(SVFGEdge* edge) override;
            bool propAlongDirectEdge(const DirectSVFGEdge* edge);
            bool propAlongIndirectEdge(const IndirectSVFGEdge* edge);
            bool propagateFromAPToFP(const ActualParmSVFGNode* ap, const SVFGNode* dst);
            bool propagateFromFRToAR(const FormalRetSVFGNode* fr, const SVFGNode* dst);
            bool propVarPtsFromSrcToDst(NodeID var, const SVFGNode* src, const SVFGNode* dst);


            size_t getPointerLevel(SVFGNode *node);
            size_t getPointerLevel(NodeID nId);
            size_t getPointerLevelFromPagNodeId(NodeID nId);


            inline const DFInOutMap& getDFInputMap() const
            {
                return getMutDFPTDataTy()->getDFIn();
            }
            inline const DFInOutMap& getDFOutputMap() const
            {
                return getMutDFPTDataTy()->getDFOut();
            }

            inline const PointsTo& getDFInPtsSet(const SVFGNode* stmt, const NodeID node)
            {
                return getDFPTDataTy()->getDFInPtsSet(stmt->getId(),node);
            }
            inline const PointsTo& getDFOutPtsSet(const SVFGNode* stmt, const NodeID node)
            {
                return getDFPTDataTy()->getDFOutPtsSet(stmt->getId(),node);
            }



            u32_t numOfProcessedAddr;	/// Number of processed Addr node
            u32_t numOfProcessedCopy;	/// Number of processed Copy node
            u32_t numOfProcessedGep;	/// Number of processed Gep node
            u32_t numOfProcessedPhi;	/// Number of processed Phi node
            u32_t numOfProcessedLoad;	/// Number of processed Load node
            u32_t numOfProcessedStore;	/// Number of processed Store node
            u32_t numOfProcessedActualParam;	/// Number of processed actual param node
            u32_t numOfProcessedFormalRet;	/// Number of processed formal ret node
            u32_t numOfProcessedMSSANode;	/// Number of processed mssa node
            

            double solveTime;	///< time of solve.
            double sccTime;	///< time of SCC detection.
            double processTime;	///< time of processNode.
            double propagationTime;	///< time of points-to propagation.
            double directPropaTime;	///< time of points-to propagation of address-taken objects
            double indirectPropaTime; ///< time of points-to propagation of top-level pointers
            double updateTime;	///< time of strong/weak updates.
            double addrTime;	///< time of handling address edges
            double copyTime;	///< time of handling copy edges
            double gepTime;	///< time of handling gep edges
            double loadTime;	///< time of load edges
            double storeTime;	///< time of store edges
            double phiTime;	///< time of phi nodes.
            double updateCallGraphTime; ///< time of updating call graph

            NodeBS svfgHasSU;

            u32_t maxSCCSize;
            u32_t numOfSCC;
            u32_t numOfNodesInSCC;

            PointsToGraphSCC *ptgScc;
            PointsToGraph *ptg;
            std::map<NodeID, size_t> pointerLevelMap;
            std::map<size_t, std::set<NodeID>> pointerLevelToNodeIDsMap;

            SVFGBuilder memSSA;
            SVFG* svfg;

            size_t currentPointerLevel;

            std::vector<NodeID> allNodes;

            size_t computeMaxPointerLevel(std::map<NodeID, std::set<NodeID>>&);

        private:
            Steensgaard *steen;
            
            
            

            AndersenWaveDiff *ander;


            

            
            size_t computePointerLevel(NodeID id, std::map<NodeID, std::set<NodeID>>&);
            inline AndersenWaveDiff* getPreAnalysis(){
                //JH todo: update to steen
                return ander;
            }




    };
}




#endif