// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "PHOSCalibWorkflow/PHOSCalibCollector.h"

#include "DetectorsCalibration/Utils.h"
#include "Framework/Task.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"

using namespace o2::framework;

void PHOSCalibCollector::init(o2::framework::InitContext& ic){

  bool isTFsendingPolicy = ic.options().get<bool>("tf-sending-policy");
  bool isTest = ic.options().get<bool>("running-in-test-mode");
  mEvent=0;
  //Create output histograms
  int nChannels=
  int nMass=150.;
  float massMax=0.3; 
  //First create all histograms then get direct pointers
  mHistos.emplace_back("hReInvMassPerCell","Real inv. mass per cell",nChannels,0.,nChannels,nMass,0.,massMax); 
  mHistos.emplace_back("hMiInvMassPerCell","Mixed inv. mass per cell",nChannels,0.,nChannels,nMass,0.,massMax); 

  int npt=45 ; 
  float xpt[46]={0.1,0.2,0.3,0.4,0.5, 0.6,0.7,0.8,0.9,1., 1.2,1.4,1.6,1.8,2., 2.2,2.4,2.6,2.8,3., 3.4,3.8,4.2,4.6,5.,  
                 5.5,6.,6.5,7.,7.5,   8.,9.,10.,12.,14.,  16.,18.,20.,24.,28., 32.,36.,40.,50.,55.,  60.} ;
  mHistos.emplace_back("hReInvMassNonlin","Real inv. mass vs Eclu",nMass,0.,massMax,npt,xpt); 
  mHistos.emplace_back("hMiInvMassNonlin","Mixed inv. mass vs Eclu",nMass,0.,massMax,npt,xpt); 

  int nTime=200;
  float timeMin=-100.e-9; 
  float timeMax= 100.e-9; 
  mHistos.emplace_back("hTimeHGPerCell","time per cell, high gain",nChannels,0.,nChannels,nTime,timeMin,timeMax); 
  mHistos.emplace_back("hTimeLGPerCell","time per cell, low gain",nChannels,0.,nChannels,nTime,timeMin,timeMax); 
  mHistos.emplace_back("hTimeHGSlewing","time vs E, high gain",nTime,timeMin,timeMax,npt,xpt); 
  mHistos.emplace_back("hTimeLGSlewing","time vs E, low gain", nTime,timeMin,timeMax,npt,xpt); 

  RungBuffer mEventBuffer;                           // Buffer for current and previous events

}

void PHOSCalibCollector::run(o2::framework::ProcessingContext& pc){

  auto tfcounter = o2::header::get<o2::framework::DataProcessingHeader*>(pc.inputs().get("input").header)->startTime; // is this the timestamp of the current TF?
  auto clusters = pc.inputs().get<gsl::span<o2::phos::FullCluster>>("clusters");
  auto cluTR    = pc.inputs().get<gsl::span<o2::phos::TriggerRecord>>("cluTR");
  std::vector<o2::phos::TriggerRecord> mOutputClusterTrigRecs;
  LOG(INFO) << "Processing TF " << tfcounter << " with " << clusters.size() << " clusters and " << cluTR.size() << " TriggerRecords";

  for(auto & tr: cluTR){

    //Mark new event
    EventHeader h={.mMarker=16383; mVtxBin.=0; .mEventId=mEvent} ;
    mDigits.push_back(h.mDataWord) ;
    
    int iclu=0;
    int firstCluInEvent = tr.getFirstEntry();
    int lastCluInEvent = firstCluInEvent + tr.getNumberOfObjects();
    
    nEventStart= mEventBuffer.size() ; //Remember "size" of circle buffer 

    for(int i=firstCluInEvent; i<lastCluInEvent; i++){
      FullCluster &clu= clusters[i] ;
      bool isGood = CheckCluster(clu) ;

      // Fill time distributions only for cells in cluster
      // Fill calibration for all cells, even bad, but partners in inv, mass should be good
      for(const CluElement ce: clu.getElementList()){
        short absId = ce.absId;
        if(ce.isHG){
          if(ce.energy>kEminHGTime){
            mHistos[kTimeHGPerCell]->Fill(absId,ce.time) ; 
          }
          mHistos[kTimeHGSlewing]->Fill(ce.time,ce.energy)
        }
        else{
          if(ce.energy>kEminLGTime){
            mHistos[kTimeLGPerCell]->Fill(absId,ce.time) ;          
          }
          mHistos[kTimeLGSlewing]->Fill(ce.time,ce.energy)
        }
      
        //Fill cells from cluster for next iterations
        short adcCounts= ce.energy/mCalibParams->GetGain(absId) ;
        if(!ce.isHG){
          adcCounts/=mCalibParams->GetHGLGRatio(absId) ;
        }
        CalibDigit d={.mAddress=absId,.mAdcAmp=adcCounts,.mHgLg =ce.isHG,.mBadChannel=isGood,.mCluster=i-firstCluInEvent} ; 
        if(i-firstCluInEvent<63){
          mDigits.push_back(d.mDataWord) ;
        }
        else{
          LOG(ERROR)<< "Too many clusters per event:" <<i-firstCluInEvent <<", apply more severe cut" ;
        }
      }

      //Real and Mixed inv mass distributions
      TLorentzVector v(px,py,pz,e) ;
      for(short ip=mBuffer.size(); --ip; ){
        const TLorentzVector &vp = mBuffer.getEntry(ip) ;
        TLorentzVector sum=v+vp;
        if(ip>nEventStart){ //same (real) event
          if(isGood){
            mHistos[kReInvMassNonlin]->Fill(e,sum.M()) ;
          }
          if(sum.Pt()>kPtMin){
            mHistos[kReInvMassPerCell]->Fill(absId,sum.M()) ;
          }
        }
        else{ //Mixed
          if(isGood){
            mHistos[kMiInvMassNonlin]->Fill(e,sum.M()) ;
          }
          if(sum.Pt()>kPtMin){
            mHistos[kMiInvMassPerCell]->Fill(absId,sum.M()) ;
          }
        }
      }

      //Add to list ot partners only if cluster is good
      if(isGood){
        nEventStart+= mEventBuffer.emplace_back(v) ; //do not change nEvent Start if buffer still increases
        nEventStart--;                               //or decrease if buffer already full
      }

    }
    mEvent++ ;
  }
  sendOutput(pc.outputs());
}

void PHOSCalibCollector::endOfStream(o2::framework::EndOfStreamContext& ec){
  //Write Filledhistograms and estimate mean number of entries
  ...
}

void PHOSCalibCollector::sendOutput(DataAllocator& output)  {
    // in output we send the calibration tree

    auto& collectedInfo = mCollector->getCollectedCalibInfo();
    LOG(DEBUG) << "In CollectorSpec sendOutput: size = " << collectedInfo.size();
    if (collectedInfo.size()) {
      auto entries = collectedInfo.size();
      // this means that we are ready to send the output
      auto entriesPerChannel = mCollector->getEntriesPerChannel();
      output.snapshot(Output{o2::header::gDataOriginTOF, "COLLECTEDINFO", 0, Lifetime::Timeframe}, collectedInfo);
      output.snapshot(Output{o2::header::gDataOriginTOF, "ENTRIESCH", 0, Lifetime::Timeframe}, entriesPerChannel);
      mCollector->initOutput(); // reset the output for the next round
    }
}


o2::framework::DataProcessorSpec getPHOSCalibCollectorDeviceSpec() ;
{
  using device = o2::phos::PHOSCalibCollectorDevice;
  using clbUtils = o2::calibration::Utils;

  std::vector<OutputSpec> outputs;
  outputs.emplace_back(o2::header::gDataOriginPHOS, "COLLECTEDINFO", 0, Lifetime::Timeframe);
  // or should I use the ConcreteDataTypeMatcher? e.g.: outputs.emplace_back(ConcreteDataTypeMatcher{clbUtils::gDataOriginCLB, clbUtils::gDataDescriptionCLBInfo});
  outputs.emplace_back(o2::header::gDataOriginPHOS, "ENTRIESCH", 0, Lifetime::Timeframe);

  std::vector<InputSpec> inputs;
  inputs.emplace_back("clusters", "PHS", "CLUSTERS");
  inputs.emplace_back("cluTR", "PHS", "CLUSTERTRIGRECS");

  return DataProcessorSpec{
    "calib-phoscalib-collector",
    inputs,
    outputs,
    AlgorithmSpec{adaptFromTask<device>()},
    Options{
      {"max-number-hits-to-fill-tree", VariantType::Int, 500, {"maximum number of entries in one channel to trigger teh filling of the tree"}},
      {"is-max-number-hits-to-fill-tree-absolute", VariantType::Bool, false, {"to decide if we want to multiply the max-number-hits-to-fill-tree by the number of channels (when set to true), or not (when set to false) for fast checks"}},
      {"tf-sending-policy", VariantType::Bool, false, {"if we are sending output at every TF; otherwise, we use the max-number-hits-to-fill-tree"}},
      {"running-in-test-mode", VariantType::Bool, false, {"to run in test mode for simplification"}}}};
}

