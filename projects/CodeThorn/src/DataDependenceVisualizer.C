// Author: Markus Schordan, 2013.

#include "rose.h"
#include "DataDependenceVisualizer.h"
#include "VariableIdUtils.h"
#include "CFAnalyzer.h"

class VariableIdSetAttribute;

// public

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
DataDependenceVisualizer::DataDependenceVisualizer(Labeler* labeler, VariableIdMapping* varIdMapping, string useDefAttributeName)
  : _showSourceCode(true),
    _labeler(labeler),
    _variableIdMapping(varIdMapping),
    _useDefAttributeName(useDefAttributeName),
    _mode(DDVMODE_DEFUSE),
    _flow(0),
    _dotFunctionClusters("")
{
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
VariableIdSet DataDependenceVisualizer::useVars(SgNode* expr) {
  UDAstAttribute* useDefAttribute=getUDAstAttribute(expr,_useDefAttributeName);
  ROSE_ASSERT(useDefAttribute);
  return useDefAttribute->useVariables(*_variableIdMapping);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
LabelSet DataDependenceVisualizer::defLabels(SgNode* expr, VariableId useVar) {
  UDAstAttribute* useDefAttribute=getUDAstAttribute(expr,_useDefAttributeName);
  ROSE_ASSERT(useDefAttribute);
  return useDefAttribute->definitionsOfVariable(useVar);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
Label DataDependenceVisualizer::getLabel(SgNode* stmt) {
  return _labeler->getLabel(stmt);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
SgNode* DataDependenceVisualizer::getNode(Label label) {
  return _labeler->getNode(label);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
string DataDependenceVisualizer::nodeSourceCode(Label lab) {
  if(_labeler->isFunctionEntryLabel(lab))
    return "FunctionEntry";
  if(_labeler->isFunctionExitLabel(lab))
    return "FunctionExit";
  // all other cases
  return SgNodeHelper::doubleQuotedEscapedString(getNode(lab)->unparseToString());
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
void DataDependenceVisualizer::generateDefUseDotGraph(SgNode* root, string fileName) {
  _mode=DDVMODE_DEFUSE;
  generateDot(root,fileName);
}
/*! 
  * \author Markus Schordan
  * \date 2013.
 */
void DataDependenceVisualizer::generateUseDefDotGraph(SgNode* root, string fileName) {
  _mode=DDVMODE_USEDEF;  
  generateDot(root,fileName);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
void DataDependenceVisualizer::generateDotFunctionClusters(SgNode* root, CFAnalyzer* cfanalyzer, string fileName, bool withDataDependencies) {

  // temporary combersome recomputation
  Flow flow=cfanalyzer->flow(root);
  LabelSet entryLabels=cfanalyzer->functionEntryLabels(flow);
  InterFlow iflow=cfanalyzer->interFlow(flow);

  std::ofstream myfile;
  myfile.open(fileName.c_str(),std::ios::out);
  myfile<<"digraph DataDependence {"<<endl;
  int k=0;
  stringstream accessToGlobalVariables; // NOT handling static vars of other functions yet
  for(LabelSet::iterator i=entryLabels.begin();i!=entryLabels.end();++i) {
    stringstream ss;
    SgNode* fdefNode=_labeler->getNode(*i);
    string functionName=SgNodeHelper::getFunctionName(fdefNode);
    ss<<"subgraph cluster_"<<(k++)<<" { label=\""<<functionName<<"\"style=filled; color=lightgrey; \n";
    Flow flow=cfanalyzer->flow(fdefNode);
    flow.setDotOptionHeaderFooter(false);
    ss<<flow.toDot(_labeler);
    myfile<<ss.str();
    if(withDataDependencies) {
      Label entryLabel=*i;
      LabelSet functionLabelSet=cfanalyzer->functionLabelSet(entryLabel,flow);
      for(LabelSet::iterator fl=functionLabelSet.begin();
          fl!=functionLabelSet.end();
          ++fl) {
        Label lab=*fl;
        SgNode* node=_labeler->getNode(lab);
        if(existsUDAstAttribute(node,_useDefAttributeName)) {
        VariableIdSet useVarSet=useVars(node);
        for(VariableIdSet::iterator i=useVarSet.begin();i!=useVarSet.end();++i) {
          VariableId useVar=*i;
          LabelSet defLabSet=defLabels(node,useVar);
          for(LabelSet::iterator i=defLabSet.begin();i!=defLabSet.end();++i) {
            Label sourceNode=lab;
            Label targetNode=*i;
            string edgeAnnotationString=_variableIdMapping->uniqueShortVariableName(useVar);
            stringstream ddedge;
            switch(_mode) {
            case DDVMODE_USEDEF: ddedge<<sourceNode<<" -> "<<targetNode; break;
            case DDVMODE_DEFUSE: ddedge<<targetNode<<" -> "<<sourceNode; break;
            default: 
              cerr<<"Error: unknown visualization mode."<<endl;
              exit(1);
            }
            if(_showSourceCode) {
              ddedge<<"[label=\""<<edgeAnnotationString<<"\" color=darkgoldenrod4];";
            }
            ddedge<<endl;
            if(functionLabelSet.isElement(sourceNode) && functionLabelSet.isElement(targetNode)) {
              myfile<<ddedge.str();
            } else {
              accessToGlobalVariables<<ddedge.str();
            }
            //            if(_showSourceCode) {
            //  myfile<<sourceNode<<" [label=\""<<sourceNode<<":"<<nodeSourceCode(sourceNode)<<"\"];"<<endl;
            //  myfile<<targetNode<<" [label=\""<<targetNode<<":"<<nodeSourceCode(targetNode)<<"\"];"<<endl;
            }
            
          }
        }
      }
    }
    myfile<<"}\n";
  }
  // generate interprocedural edges
  for(InterFlow::iterator i=iflow.begin();i!=iflow.end();++i) {
    if(((*i).call != Labeler::NO_LABEL) && ((*i).entry!= Labeler::NO_LABEL))
      myfile<<(*i).call<<" -> "<<(*i).entry<<";\n";
    if(((*i).exit != Labeler::NO_LABEL) && ((*i).callReturn!= Labeler::NO_LABEL))
      myfile<<(*i).exit<<" -> "<<(*i).callReturn<<";\n";
    // generate optional local edge
    if(((*i).call != Labeler::NO_LABEL) && ((*i).callReturn!= Labeler::NO_LABEL))
      myfile<<(*i).call<<" -> "<<(*i).callReturn<<" [style=dotted];\n";
  }
  myfile<<"// access to global variables\n";
  myfile<<accessToGlobalVariables.str();
  myfile<<"}"<<endl;
  myfile.close();
}
/*! 
  * \author Markus Schordan
  * \date 2013.
 */
void DataDependenceVisualizer::generateDot(SgNode* root, string fileName) {
  std::ofstream myfile;
  myfile.open(fileName.c_str(),std::ios::out);
  myfile<<"digraph DataDependence {"<<endl;
#if 1
  long labelNum=_labeler->numberOfLabels(); // may change this to Flow. (make it dependent, whether _flow exists)
#else
  long labelNum=_labeler->numberOfLabels();
#endif
  for(long i=0;i<labelNum;++i) {
    Label lab=i;
    SgNode* node=_labeler->getNode(i);
    if(!node)
      continue;
    if(existsUDAstAttribute(node,_useDefAttributeName)) {
      VariableIdSet useVarSet=useVars(node);
      for(VariableIdSet::iterator i=useVarSet.begin();i!=useVarSet.end();++i) {
        VariableId useVar=*i;
        LabelSet defLabSet=defLabels(node,useVar);
        for(LabelSet::iterator i=defLabSet.begin();i!=defLabSet.end();++i) {
          Label sourceNode=lab;
          Label targetNode=*i;
          string edgeAnnotationString=_variableIdMapping->uniqueShortVariableName(useVar);
          switch(_mode) {
          case DDVMODE_USEDEF:
            myfile<<sourceNode<<" -> "<<targetNode; break;
          case DDVMODE_DEFUSE:
            myfile<<targetNode<<" -> "<<sourceNode; break;
          default: 
            cerr<<"Error: unknown visualization mode."<<endl;
            exit(1);
          }
          if(_showSourceCode) {
            myfile<<"[label=\""<<edgeAnnotationString<<"\" color=darkgoldenrod4];";
          }
          myfile<<endl;
          
          if(_showSourceCode) {
            myfile<<sourceNode<<" [label=\""<<sourceNode<<":"<<nodeSourceCode(sourceNode)<<"\"];"<<endl;
            myfile<<targetNode<<" [label=\""<<targetNode<<":"<<nodeSourceCode(targetNode)<<"\"];"<<endl;
          }
          
        }
      }
    }
  }
  if(_flow) {
    _flow->setDotOptionHeaderFooter(false);
    //_flow->setDotOptionDisplayLabel(true);
    //_flow->setDotOptionDisplayStmt(true);
    //_flow->setDotOptionFixedColor(true);
    myfile<<_flow->toDot(_labeler);
  }
  myfile<<"}"<<endl;
  myfile.close();
}

// private

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
UDAstAttribute* DataDependenceVisualizer::getUDAstAttribute(SgNode* expr,string attributeName){
  if(existsUDAstAttribute(expr,attributeName)) {
    UDAstAttribute* udAttr=dynamic_cast<UDAstAttribute*>(expr->getAttribute(attributeName));
    return udAttr;
  } else {
    return 0;
  }
}
/*! 
  * \author Markus Schordan
  * \date 2013.
 */
bool DataDependenceVisualizer::existsUDAstAttribute(SgNode* expr,string attributeName){
  return expr->attributeExists(attributeName);
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
void DataDependenceVisualizer::setFunctionLabelSetSets(LabelSetSet functionLabelSetSets) {
  _functionLabelSetSets=functionLabelSetSets;
}

#if 0
void DataDependenceVisualizer::generateDotFunctionClusters(LabelSetSet functionLabelSetSet) {
  stringstream ss;
  ss<<"// function clusters\n";
  int k=0;
  for(LabelSetSet::iterator i=functionLabelSetSet.begin();i!=functionLabelSetSet.end();++i) {
    cout<<"Generating cluster "<<k<<endl;
    ss<<"subgraph cluster_"<<(k++)<<" { style=filled; color=lightgrey; ";
    for(LabelSet::iterator j=(*i).begin();j!=(*i).end();++j) {
      ss<<(*j)<<" ";
    }
    ss<<"}\n";
  }
  _dotFunctionClusters=ss.str();
}
#endif
