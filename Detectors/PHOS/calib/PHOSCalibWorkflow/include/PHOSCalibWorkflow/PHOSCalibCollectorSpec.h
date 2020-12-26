// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CALIBRATION_PHOSCALIB_COLLECTOR_H
#define O2_CALIBRATION_PHOSCALIB_COLLECTOR_H

/// @file   PHOSCalibCollectorSpec.h
/// @brief  Device to collect information for PHOS time slewing calibration.

#include "PHOSCalibration/PHOSCalibCollector.h"
#include "DetectorsCalibration/Utils.h"
#include "DataFormatsPHOS/CalibInfoPHOS.h"
#include "Framework/Task.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"

using namespace o2::framework;

namespace o2
{
namespace phos
{

class PHOSCalibCollectorDevice : public o2::framework::Task
{

 public:
  void init(o2::framework::InitContext& ic) final ;

  void run(o2::framework::ProcessingContext& pc) final ;

  void endOfStream(o2::framework::EndOfStreamContext& ec) final ;

 private:
  void sendOutput(DataAllocator& output) ;

  std::unique_ptr<o2::phos::PHOSCalibCollector> mCollector;
  int mMaxNumOfHits = 0;

};

} // namespace phos

o2::framework :: DataProcessorSpec getPHOSCalibCollectorDeviceSpec() ;

}
#endif
