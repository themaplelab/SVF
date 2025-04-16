

#ifndef LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_
#define LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_

#include "Graphs/SVFGOPT.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAFSSolver.h"
#include "Graphs/PointsToGraph.h"
#include "MSSA/SVFGBuilder.h"
#include <iostream>





namespace SVF{

    // class AndersenWaveDiff;
    // class SVFModule;

    class Steensgaard;
    class AndersenWaveDiff;


    typedef WPAFSSolver<SVFG*> WPASVFGFSSolver;

    class LevelByLevelFlowSensitive : public WPASVFGFSSolver, public BVDataPTAImpl{

        public:
            // JH todo : add PointToGraph
            typedef SCCDetection<PointsToGraph*> PointsToGraphSCC;

            // JH todo: update type of pag
            explicit LevelByLevelFlowSensitive(SVFIR* _pag, PTATY type = FSSPARSE_WPA) : WPASVFGFSSolver(), BVDataPTAImpl(_pag, type), ptgScc(nullptr){
                
            }

            void analyze() override;
            void initialize() override;
            void solveConstraints();
            void finalize() override;



            // JH todo: delete ptg and other memory 
            ~LevelByLevelFlowSensitive() override = default;

            inline void pointsToGraphSCCDetection(){
                
                if(ptgScc == nullptr){
                    ptgScc = new PointsToGraphSCC(ptg);
                }
                ptgScc->find();            
            }

            size_t computePointerLevels(std::map<NodeID, std::set<NodeID>>&);
            size_t getPointerLevel(NodeID id, std::map<NodeID, std::set<NodeID>>&);

        protected:
            // virtual inline void initWorklist() override
            // {
            //     NodeStack& nodeStack = SCCDetect();
            //     while (!nodeStack.empty())
            //     {
            //         NodeID nodeId = nodeStack.top();
            //         nodeStack.pop();
            //         std::cout << "push svfg node " << nodeId << "\n";
            //         pushIntoWorklist(nodeId);
            //     }
            // }

            bool updateCallGraph(const CallSiteToFunPtrMap& callsites) override;

            virtual inline void initWorklist() override{
                std::cout << "aaaa" << "\n";

                if(allNodes.empty()){
                    NodeStack &nodeStack = SCCDetect();
                    std::cout << nodeStack.size() << "\n";
                    while (!nodeStack.empty())
                    {
                        NodeID nodeId = nodeStack.top();
                        nodeStack.pop();
                        allNodes.push_back(nodeId);
                        // pushIntoWorklist(nodeId);
                    }
                }
                for(auto nId : allNodes){
                    if(getPointerLevel(nId) == currentPointerLevel){
                        std::cout << "push svfg node " << nId << " with pointer level " << getPointerLevel(nId) << "\n";

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
            bool processLoad(const LoadSVFGNode* load);
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




        private:
            Steensgaard *steen;
            PointsToGraphSCC *ptgScc;
            PointsToGraph *ptg;
            std::map<NodeID, size_t> pointerLevelMap;
            std::map<size_t, std::set<NodeID>> pointerLevelToNodeIDsMap;
            SVFGBuilder memSSA;
            SVFG* svfg;
            size_t currentPointerLevel;

            std::vector<NodeID> allNodes;


            AndersenWaveDiff *ander;



    };
}




#endif