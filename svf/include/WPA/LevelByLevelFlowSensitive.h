

#ifndef LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_
#define LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_H_

#include "Graphs/SVFGOPT.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAFSSolver.h"
#include "Graphs/PointsToGraph.h"




namespace SVF{

    // class AndersenWaveDiff;
    // class SVFModule;

    class Steensgaard;


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

            // JH todo: delete ptg and other memory 
            ~LevelByLevelFlowSensitive() override = default;

            inline void pointsToGraphSCCDetection(){
                
                if(ptgScc == nullptr){
                    ptgScc = new PointsToGraphSCC(ptg);
                }
                ptgScc->find();            
            }



        private:
            Steensgaard *steen;
            PointsToGraphSCC *ptgScc;
            PointsToGraph *ptg;


    };
}




#endif