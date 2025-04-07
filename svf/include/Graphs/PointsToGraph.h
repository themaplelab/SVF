

#ifndef GRAPH_POINTSTOGRAPH_H_
#define GRAPH_POINTSTOGRAPH_H_


#include "Graphs/GenericGraph.h"
#include "SVFIR/SVFVariables.h"



namespace SVF{

    class PointsToGraphNode;

    typedef GenericEdge<PointsToGraphNode> GenericPointsToGraphEdgeTy;
    class PointsToGraphEdge : public GenericPointsToGraphEdgeTy{
        public:
            enum PtEDGEK{
                PointsTo
            };

            // JH todo: add new node type for point-to graph

            PointsToGraphEdge(PointsToGraphNode *src, PointsToGraphNode *dst, PtEDGEK kind) : GenericPointsToGraphEdgeTy(src, dst, kind) {}

            virtual ~PointsToGraphEdge(){}

            static inline bool classof(const PointsToGraphNode*){
                return true;
            }

    };

    typedef GenericNode<PointsToGraphNode, PointsToGraphEdge> GenericPointsToGraphNodeTy;
    class PointsToGraphNode : public GenericPointsToGraphNodeTy{

        public:
            PointsToGraphNode(NodeID i) : GenericPointsToGraphNodeTy(i,OtherKd){
                
            }

            static inline bool classof(const PointsToGraphNode*){
                return true;
            }
            

            

            

    };

    class Steensgaard;
    class AndersenWaveDiff;


    typedef GenericGraph<PointsToGraphNode, PointsToGraphEdge> GenericPointsToGraphTy;
    class PointsToGraph : public GenericPointsToGraphTy{

        protected:
            void destroy(){

            }

        public:
            PointsToGraph(Steensgaard *steen);
            PointsToGraph(AndersenWaveDiff *ander);


            /// Add call graph edge
            inline void addEdge(PointsToGraphEdge* edge){
                edge->getDstNode()->addIncomingEdge(edge);
                edge->getSrcNode()->addOutgoingEdge(edge);
            }

            /// Destructor
            virtual ~PointsToGraph(){
                destroy();
            }

            inline PointsToGraphNode* getCallGraphNode(NodeID id) const{
                return getGNode(id);
            }

            PointsToGraphEdge* hasGraphEdge(PointsToGraphNode* src, PointsToGraphNode* dst, PointsToGraphEdge::PtEDGEK kind) const{
                PointsToGraphEdge edge(src,dst,kind);
                PointsToGraphEdge* outEdge = src->hasOutgoingEdge(&edge);
                PointsToGraphEdge* inEdge = dst->hasIncomingEdge(&edge);
                if (outEdge && inEdge){
                    assert(outEdge == inEdge && "edges not match");
                    return outEdge;
                }
                else{
                    return nullptr;
                }
                    
            }

            PointsToGraphEdge* getGraphEdge(PointsToGraphNode* src, PointsToGraphNode* dst){
                for (auto iter = src->OutEdgeBegin(); iter != src->OutEdgeEnd(); ++iter){
                    PointsToGraphEdge* edge = (*iter);
                    if(edge->getDstID() == dst->getId()){
                        return edge;
                    }      
                }
                return nullptr;
            }



        private:
            Map<NodeID, PointsToGraphNode*> idToNodeMap;
            static size_t id;

    };



}



namespace SVF{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::PointsToGraphNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::PointsToGraphNode,SVF::PointsToGraphEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::PointsToGraphNode*> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::PointsToGraphNode,SVF::PointsToGraphEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::PointsToGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::PointsToGraphNode,SVF::PointsToGraphEdge>* >
{
    typedef SVF::PointsToGraphNode* NodeRef;
};

} // End namespace llvm





#endif