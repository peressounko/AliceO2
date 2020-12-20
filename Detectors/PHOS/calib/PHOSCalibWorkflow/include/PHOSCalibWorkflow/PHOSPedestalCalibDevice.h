// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CALIBRATION_PHOSPEDESTALS_CALIBRATOR_H
#define O2_CALIBRATION_PHOSPEDESTALS_CALIBRATOR_H

/// @file   PedestalCalibSpec.h
/// @brief  Device to calculate PHOS pedestals

#include "Framework/Task.h"
// #include "Framework/ConfigParamRegistry.h"
// #include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"
#include "PHOSReconstruction/CaloRawFitter.h"
#include "PHOSBase/Mapping.h"
#include "PHOSCalib/Pedestals.h"
#include "TH2.h"

using namespace o2::framework;

namespace o2
{
namespace phos
{

class PHOSPedestalCalibDevice : public o2::framework::Task
{

 public:
  explicit PHOSPedestalCalibDevice(bool useCCDB) : mUseCCDB(useCCDB) {}
  void init(o2::framework::InitContext& ic) final ;

  void run(o2::framework::ProcessingContext& pc) final ;

  void endOfStream(o2::framework::EndOfStreamContext& ec) final ;

 protected: 
  void sendOutput(DataAllocator& output) ;
  // void evaluateMeans

 private:

  bool mUseCCDB = false;

  CaloRawFitter *mRawFitter = nullptr ;  /// Sample fitting class
  Mapping  *mMapping = nullptr;          ///!<! Mapping
  Pedestals *mPedestalObject=nullptr ;   /// Final calibration object
  TH2F *mMeanHG = nullptr ;              /// Mean values in High Gain channels
  TH2F *mMeanLG = nullptr ;              /// RMS of values in High Gain channels
  TH2F *mRMSHG  = nullptr ;              /// Mean values in Low Gain channels
  TH2F *mRMSLG  = nullptr ;              /// RMS of values in Low Gain channels
  ClassDefNV(PHOSPedestalCalibDevice, 1);
};

o2::framework::DataProcessorSpec getPedestalCalibSpec(bool useCCDB) ;

} // namespace phos
} // namespace o2

#endif
