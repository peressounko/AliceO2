// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "CPVCalibWorkflow/CPVPedestalCalibDevice.h"
#include "CCDB/CcdbApi.h"
#include "CCDB/CcdbObjectInfo.h"
#include <string>
#include "FairLogger.h"
#include "CommonDataFormat/InteractionRecord.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"
#include "DataFormatsCPV/TriggerRecord.h"
#include "DetectorsRaw/RDHUtils.h"
#include "Framework/InputRecordWalker.h"

using namespace o2::cpv;


void CPVPedestalCalibDevice::init(o2::framework::InitContext& ic){

  //Create histograms for mean and RMS
  short n=3*o2::cpv::Geometry::kNumberOfCPVPadsPhi * o2::cpv::Geometry::kNumberOfCPVPadsZ;

  mMean = new TH2F("Mean","Mean",n,0.5,n+0.5,100,0.,100.) ;

}

void CPVPedestalCalibDevice::run(o2::framework::ProcessingContext& ctx){

  //
  for (const auto& rawData : framework::InputRecordWalker(ctx.inputs())) {   

   o2::cpv::RawReaderMemory rawreader(o2::framework::DataRefUtils::as<const char>(rawData));
    // loop over all the DMA pages
    while (rawreader.hasNext()) {
      try {
        rawreader.next();
      } catch (RawErrorType_t e) {
        LOG(ERROR) << "Raw decoding error " << (int)e;
        //add error list
        mOutputHWErrors.emplace_back(5, 0, 0, 0, e ); //Put general errors to non-existing DDL5 
        //if problem in header, abandon this page
        if (e == RawErrorType_t::kPAGE_NOTFOUND ||
            e == RawErrorType_t::kHEADER_DECODING ||
            e == RawErrorType_t::kHEADER_INVALID) {
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
      std::shared_ptr<std::vector<o2::cpv::Digit>> currentDigitContainer;
      auto found = digitBuffer.find(currentIR);
      if (found == digitBuffer.end()) {
        currentDigitContainer = std::make_shared<std::vector<o2::cpv::Digit>>();
        digitBuffer[currentIR] = currentDigitContainer;
      } else {
        currentDigitContainer = found->second;
      }
      //
      if (ddl > o2::cpv::Geometry::kNDDL) { //only 4 correct DDLs
        LOG(ERROR) << "DDL=" << ddl;
        continue;                                        //skip STU ddl
      }
      // use the altro decoder to decode the raw data, and extract the RCU trailer
      o2::cpv::RawDecoder decoder(rawreader);
      RawErrorType_t err = decoder.decode();
      if (err != kOK) {
        //TODO handle severe errors
      }
      // Loop over all the channels
      for (uint32_t adch : decoder.getDigits()) {
        AddressCharge ac={adch} ;
        unsigned short absId=ac.Address ;
        mMean->Fill(absId, ac.Charge );
      }
    } //RawReader::hasNext
  }
}

void CPVPedestalCalibDevice::endOfStream(o2::framework::EndOfStreamContext& ec){

  LOG(INFO) << "[CPVPedestalCalibDevice - endOfStream]" ;
  //calculate stuff here


  sendOutput(ec.outputs());

}

void CPVPedestalCalibDevice::sendOutput(DataAllocator& output){
    // extract CCDB infos and calibration objects, convert it to TMemFile and send them to the output
    // TODO in principle, this routine is generic, can be moved to Utils.h
    // using clbUtils = o2::calibration::Utils;
    std::map<std::string, std::string> metadataChannelCalib; 
    LOG(INFO) << "Sending object CPV/Calib/Pedestals" ;
    // output.snapshot(Output{"CPV/Calib", metadataChannelCalib}, mPedestalObject); 

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


o2::framework::DataProcessorSpec o2::cpv::getPedestalCalibSpec(bool useCCDB)
{
 
  std::vector<o2::framework::OutputSpec> outputs;
  outputs.emplace_back("CPV", "PEDCALIBS", 0, o2::framework::Lifetime::Timeframe);

  return o2::framework::DataProcessorSpec{"PedestalCalibSpec",
                                          o2::framework::select("A:CPV/RAWDATA"), 
                                          outputs,
                                          o2::framework::adaptFromTask<CPVPedestalCalibDevice>(useCCDB),
                                          o2::framework::Options{}};
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
