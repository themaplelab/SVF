#ifndef LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_OPTIMIZED_H_
#define LEVEL_BY_LEVEL_FLOW_SENSITIVE_ANALYSIS_OPTIMIZED_H_


#include "WPA/LevelByLevelFlowSensitive.h"


namespace SVF{

    class Steensgaard;
    class AndersenWaveDiff;

    class LevelByLevelFlowSensitiveOptimized : public LevelByLevelFlowSensitive{

        public:

            explicit LevelByLevelFlowSensitiveOptimized(SVFIR* _pag, PTATY type = FSSPARSE_WPA) : LevelByLevelFlowSensitive(_pag, type){
                    
            }


            virtual void initialize() override;
            virtual void solveConstraints() override;

        private:
            AndersenWaveDiff *ander;
            Steensgaard *steen;

        protected:
            virtual bool processLoad(const LoadSVFGNode* load) override;
            virtual bool processStore(const StoreSVFGNode* store) override;

            



    };



}





#endif