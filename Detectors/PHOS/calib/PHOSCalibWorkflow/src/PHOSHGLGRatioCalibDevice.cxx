// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "PHOSCalibWorkflow/PHOSHGLGRatioCalibDevice.h"
#include "CCDB/CcdbApi.h"
#include "CCDB/CcdbObjectInfo.h"
#include <string>
#include "FairLogger.h"
#include "CommonDataFormat/InteractionRecord.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"
#include "DataFormatsPHOS/TriggerRecord.h"
#include "DetectorsRaw/RDHUtils.h"
#include "Framework/InputRecordWalker.h"
#include "PHOSBase/Mapping.h"
#include "PHOSReconstruction/Bunch.h"
#include "PHOSReconstruction/AltroDecoder.h"
#include "PHOSReconstruction/RawDecodingError.h"

using namespace o2::phos;


void PHOSHGLGRatioCalibDevice::init(o2::framework::InitContext& ic){

  auto path = ic.options().get<std::string>("mappingpath");
  if (!mMapping) {
    mMapping = new o2::phos::Mapping(path);
    if (!mMapping) {
      LOG(ERROR) << "Failed to initialize mapping";
    }
    if (mMapping->setMapping() != o2::phos::Mapping::kOK) {
      LOG(ERROR) << "Failed to construct mapping";
    }
  }
  mRawFitter = new o2::phos::CaloRawFitter();


  //Create histograms for mean and RMS
  short n=o2::phos::Mapping::NCHANNELS;
  mhRatio = new TH2F("HGLGRatio","HGLGRatio",n,0.5,n+0.5,100,10.,20.) ;
}

void PHOSHGLGRatioCalibDevice::run(o2::framework::ProcessingContext& ctx){

  //
  for (const auto& rawData : framework::InputRecordWalker(ctx.inputs())) {

    o2::phos::RawReaderMemory rawreader(o2::framework::DataRefUtils::as<const char>(rawData));

    // loop over all the DMA pages
    while (rawreader.hasNext()) {
      mMapPairs.clear() ;

      try {
        rawreader.next();
      } catch (RawDecodingError::ErrorType_t e) {
        LOG(ERROR) << "Raw decoding error " << (int)e;
        //if problem in header, abandon this page
        if (e == RawDecodingError::ErrorType_t::PAGE_NOTFOUND ||
            e == RawDecodingError::ErrorType_t::HEADER_DECODING ||
            e == RawDecodingError::ErrorType_t::HEADER_INVALID) {
          break;
        }
        //if problem in payload, try to continue
        continue;
      }
      auto& header = rawreader.getRawHeader();
      auto triggerBC = o2::raw::RDHUtils::getTriggerBC(header);
      auto triggerOrbit = o2::raw::RDHUtils::getTriggerOrbit(header);
      auto ddl = o2::raw::RDHUtils::getFEEID(header);

      o2::InteractionRecord currentIR(triggerBC, triggerOrbit);

      if (ddl > o2::phos::Mapping::NDDL) { //only 14 correct DDLs
        LOG(ERROR) << "DDL=" << ddl;
        continue;                                        //skip STU ddl
      }
      // use the altro decoder to decode the raw data, and extract the RCU trailer
      o2::phos::AltroDecoder decoder(rawreader);
      AltroDecoderError::ErrorType_t err = decoder.decode();

      if (err != AltroDecoderError::kOK) {
      	LOG(ERROR)<<"Errror "<< err << " in decoding DDL" << ddl;
      }
      auto& rcu = decoder.getRCUTrailer();
      auto& channellist = decoder.getChannels();
      // Loop over all the channels for this RCU
      for (auto& chan : channellist) {
        short absId;
        Mapping::CaloFlag caloFlag;
        short fee;
        Mapping::ErrorStatus s = mMapping->hwToAbsId(ddl, chan.getHardwareAddress(), absId, caloFlag);
        if (s != Mapping::ErrorStatus::kOK) {
          LOG(ERROR)<<"Error in mapping" << " ddl=" << ddl << " hwaddress "	<< chan.getHardwareAddress() ;
          continue;
        }
        if (caloFlag != Mapping::kTRU) { //HighGain or LowGain
          CaloRawFitter::FitStatus fitResults = mRawFitter->evaluate(chan.getBunches());
          if (fitResults == CaloRawFitter::FitStatus::kOK || fitResults == CaloRawFitter::FitStatus::kNoTime) {
            for (int is = 0; is < mRawFitter->getNsamples(); is++) {
              //if map entry already exist, fill second part, otherwise fill first map
              if(mRawFitter->isOverflow(is)){
                continue;
              }
              auto search = mMapPairs.find(absId);
              if (search != mMapPairs.end()) { //exist 
                if (caloFlag == Mapping::kHighGain ) {
                  search->second.mHGAmp = mRawFitter->getAmp(is) ;
                }            
                if (caloFlag == Mapping::kLowGain ) {
                  search->second.mLGAmp = mRawFitter->getAmp(is) ;
                }            

              } else {
                PairAmp pa = {0};
                if (caloFlag == Mapping::kHighGain ) {
                  pa.mHGAmp = mRawFitter->getAmp(is) ;
                }            
                if (caloFlag == Mapping::kLowGain ) {
                  pa.mLGAmp = mRawFitter->getAmp(is) ;
                }            
                mMapPairs.insert({absId,pa}) ;
              }
            }
          }
        }
      }
      fillRatios() ;
    } //RawReader::hasNext
  }
}
void PHOSHGLGRatioCalibDevice::fillRatios(){
  // scan collected map and fill ratio histogram if both channels are filled
  for(auto &it: mMapPairs){
    if(it.second.mHGAmp>0 && it.second.mLGAmp > mMinLG){
      mhRatio->Fill(it.first,float(it.second.mHGAmp/it.second.mLGAmp)) ;
    }
  }
  mMapPairs.clear() ; //to avoid double counting
}


void PHOSHGLGRatioCalibDevice::endOfStream(o2::framework::EndOfStreamContext& ec){

  LOG(INFO) << "[PHOSHGLGRatioCalibDevice - endOfStream]" ;
  //calculate stuff here
  //.........



  sendOutput(ec.outputs());

}

void PHOSHGLGRatioCalibDevice::sendOutput(DataAllocator& output){
    // extract CCDB infos and calibration objects, convert it to TMemFile and send them to the output
    // TODO in principle, this routine is generic, can be moved to Utils.h
    // using clbUtils = o2::calibration::Utils;
    std::map<std::string, std::string> metadataChannelCalib; 
    //Read latest calib parameter and replace in it HG/LG ratio.
    //upload new one with updated ratio

    LOG(INFO) << "Sending object PHOS/Calib/Pedestals" ;
    // output.snapshot(Output{"PHOS/Calib", metadataChannelCalib}, mPedestalObject); 

}



//
// Method to compare new pedestals and latest in ccdb
// Do not update existing object automatically if difference is too strong
// create object with validity range if far future (?) and send warning (e-mail?) to operator
   // if (mUseCCDB) { // read calibration objects from ccdb
   //    LHCphase lhcPhaseObjTmp;
      /*
      // for now this part is not implemented; below, the sketch of how it should be done
      if (mAttachToLHCphase) {
        // if I want to take the LHCclockphase just produced, I need to loop over what the previous spec produces:
        int nSlots = pc.inputs().getNofParts(0);
        assert(pc.inputs().getNofParts(1) == nSlots);

        int lhcphaseIndex = -1;
        for (int isl = 0; isl < nSlots; isl++) {
          const auto wrp = pc.inputs().get<CcdbObjectInfo*>("clbInfo", isl);
          if (wrp->getStartValidityTimestamp() > tfcounter) { // replace tfcounter with the timestamp of the TF
            lhxphaseIndex = isl - 1;
            break;
          }
        }
        if (lhcphaseIndex == -1) {
          // no new object found, use CCDB
	  auto lhcPhase = pc.inputs().get<LHCphase*>("tofccdbLHCphase");
          lhcPhaseObjTmp = std::move(*lhcPhase);
        }
        else {
          const auto pld = pc.inputs().get<gsl::span<char>>("clbPayload", lhcphaseIndex); // this is actually an image of TMemFile
          // now i need to make a LHCphase object; Ruben suggested how, I did not try yet
	  // ...
        }
      }
*/

DataProcessorSpec o2::phos::getHGLGRatioCalibSpec(bool useCCDB)
{
  std::vector<o2::framework::InputSpec> inputs;
  inputs.emplace_back("RAWDATA", o2::framework::ConcreteDataTypeMatcher{"PHS", "RAWDATA"}, o2::framework::Lifetime::Timeframe);

  std::vector<o2::framework::OutputSpec> outputs;
  outputs.emplace_back("PHS", "HGLGRATIO", 0, o2::framework::Lifetime::Timeframe);

  return o2::framework::DataProcessorSpec{"PedestalCalibSpec",
                                          inputs, 
                                          outputs,
                                          o2::framework::adaptFromTask<o2::phos::PHOSHGLGRatioCalibDevice>(useCCDB),
                                          o2::framework::Options{
                                          {"mappingpath", o2::framework::VariantType::String, "", {"Path to mapping files"}}}};
}


// //============================================================
// DataProcessorSpec getTOFChannelCalibDeviceSpec(bool useCCDB){
//   using device = o2::calibration::TOFChannelCalibDevice;
//   using clbUtils = o2::calibration::Utils;

//   std::vector<OutputSpec> outputs;
//   outputs.emplace_back(ConcreteDataTypeMatcher{clbUtils::gDataOriginCLB, clbUtils::gDataDescriptionCLBPayload});
//   outputs.emplace_back(ConcreteDataTypeMatcher{clbUtils::gDataOriginCLB, clbUtils::gDataDescriptionCLBInfo});

//   std::vector<InputSpec> inputs;
//   inputs.emplace_back("input", "TOF", "CALIBDATA");
//   if (useCCDB) {
//     inputs.emplace_back("tofccdbLHCphase", o2::header::gDataOriginTOF, "LHCphase");
//     inputs.emplace_back("tofccdbChannelCalib", o2::header::gDataOriginTOF, "ChannelCalib");
//     inputs.emplace_back("startTimeLHCphase", o2::header::gDataOriginTOF, "StartLHCphase");
//     inputs.emplace_back("startTimeChCalib", o2::header::gDataOriginTOF, "StartChCalib");
//   }

//   return DataProcessorSpec{
//     "calib-tofchannel-calibration",
//     inputs,
//     outputs,
//     AlgorithmSpec{adaptFromTask<device>(useCCDB, attachChannelOffsetToLHCphase)},
//     Options{
//       {"min-entries", VariantType::Int, 500, {"minimum number of entries to fit channel histos"}},
//       {"nbins", VariantType::Int, 1000, {"number of bins for t-texp"}},
//       {"range", VariantType::Float, 24000.f, {"range for t-text"}},
//       {"do-TOF-channel-calib-in-test-mode", VariantType::Bool, false, {"to run in test mode for simplification"}},
//       {"ccdb-path", VariantType::String, "http://ccdb-test.cern.ch:8080", {"Path to CCDB"}}}};
// }
